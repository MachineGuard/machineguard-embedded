#include "Io.h"
#include "../Ack.h"
#include "../HttpHeaders.h"
#include <WiFi.h>
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <cstring>
#include <cstdio>
#include <strings.h>

namespace mg {
namespace { std::atomic<bool> synchronizedClock{false}; }
bool EdgeIo::begin(const NetworkSettings& settings,const char* ntp) {
    if(task_ || jobs_ || !validNetwork(settings))return false;
    settings_=settings;ntpServer_=ntp;
    if(!settings.enabled)return true;
    jobs_=xQueueCreate(1,sizeof(SendJob));results_=xQueueCreate(1,sizeof(SendResult));
    if(jobs_ && results_ && xTaskCreatePinnedToCore(entry,"edge-io",24576,this,1,&task_,0)==pdPASS)return true;
    if(jobs_)vQueueDelete(jobs_);if(results_)vQueueDelete(results_);
    jobs_=nullptr;results_=nullptr;return false;
}
bool EdgeIo::submit(const Sample& sample,uint32_t ticket,const char* deviceCode) {
    if(!jobs_)return false;
    SendJob job;job.sample=sample;job.ticket=ticket;std::strncpy(job.deviceCode,deviceCode,sizeof job.deviceCode-1);
    return xQueueSend(jobs_,&job,0)==pdTRUE;
}
bool EdgeIo::receive(SendResult& result) {return results_ && xQueueReceive(results_,&result,0)==pdTRUE;}
CaptureTime EdgeIo::captureTime() const {
    if(!synchronizedClock.load())return {};
    timeval time{};gettimeofday(&time,nullptr);
    const int64_t ms=int64_t(time.tv_sec)*1000+time.tv_usec/1000;
    return {ms>=MinimumEpochMs,ms};
}
void EdgeIo::entry(void* self) {static_cast<EdgeIo*>(self)->run();}
void EdgeIo::run() {
    WiFi.persistent(false);WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(false);
    if(ntpServer_ && ntpServer_[0]) {
        esp_sntp_set_time_sync_notification_cb([](timeval*){synchronizedClock.store(true);});
        configTime(0,0,ntpServer_);
    }
    uint32_t reconnectAt=millis()-10000;
    for(;;) {
        const bool online=WiFi.status()==WL_CONNECTED;wifi_.store(online);
        if(!online) {
            edge_.store(false);
            if(elapsed(millis(),reconnectAt,10000)) {reconnectAt=millis();WiFi.begin(settings_.ssid,settings_.password);}
        }
        SendJob job;
        if(xQueueReceive(jobs_,&job,pdMS_TO_TICKS(50))==pdTRUE) {
            Outcome outcome=online?exchange(job):Outcome::Retry;
            edge_.store(outcome==Outcome::LocalPending || outcome==Outcome::Confirmed || outcome==Outcome::Discarded
                || outcome==Outcome::Quarantined || outcome==Outcome::EdgeOverflow || outcome==Outcome::InvalidSample || outcome==Outcome::Conflict);
            const SendResult result{job.ticket,outcome};xQueueOverwrite(results_,&result);
        }
    }
}
Outcome EdgeIo::exchange(const SendJob& job) {
    char payload[PayloadBytes]{};if(!encodePayload(job.deviceCode,job.sample,payload,sizeof payload))return Outcome::InvalidReceipt;
    NetworkClient plain;NetworkClientSecure secure;NetworkClient* client=settings_.tls?static_cast<NetworkClient*>(&secure):&plain;
    if(settings_.tls) {secure.setCACert(settings_.caCertificate);secure.setHandshakeTimeout(3);}
    client->setTimeout(settings_.readMs);
    const uint32_t started=millis();
    if(!client->connect(settings_.host,settings_.port,int32_t(settings_.connectMs)))return Outcome::Retry;
    if(elapsed(millis(),started,settings_.exchangeMs)) {client->stop();return Outcome::Retry;}
    char request[1024]{};
    const int length=std::snprintf(request,sizeof request,
        "POST /api/v1/edge/readings HTTP/1.0\r\nHost: %s:%u\r\nX-Node-Credential: %s\r\nContent-Type: application/json\r\nContent-Length: %u\r\nConnection: close\r\n\r\n%s",
        settings_.host,unsigned(settings_.port),settings_.credential,unsigned(std::strlen(payload)),payload);
    if(length<0 || size_t(length)>=sizeof request || client->write(reinterpret_cast<const uint8_t*>(request),size_t(length))!=size_t(length)) {
        client->stop();return Outcome::Retry;
    }
    char header[2049]{};size_t used=0;uint32_t lastByte=millis();bool complete=false;
    while(used<sizeof header-1 && !elapsed(millis(),started,settings_.exchangeMs) && !elapsed(millis(),lastByte,settings_.readMs)) {
        if(client->available()) {
            const int value=client->read();if(value<0)break;
            header[used++]=char(value);lastByte=millis();
            if(used>=4 && std::memcmp(header+used-4,"\r\n\r\n",4)==0) {complete=true;break;}
        } else if(!client->connected())break;
        else vTaskDelay(pdMS_TO_TICKS(1));
    }
    HttpHeaders framing{};
    if(!complete || !parseHttpHeaders(header,used,framing)) {client->stop();return Outcome::InvalidReceipt;}
    const int status=framing.status;
    if(status==401 || status==403 || status>=500 || status==404 || status==429) {
        client->stop();return classifyResponse(status,nullptr,0,job.deviceCode,job.sample);
    }
    const size_t contentLength=framing.bodyLength;
    char body[ResponseBytes+1]{};used=0;lastByte=millis();
    while(used<size_t(contentLength) && !elapsed(millis(),started,settings_.exchangeMs) && !elapsed(millis(),lastByte,settings_.readMs)) {
        if(client->available()) {const int value=client->read();if(value<0)break;body[used++]=char(value);lastByte=millis();}
        else if(!client->connected())break;else vTaskDelay(pdMS_TO_TICKS(1));
    }
    client->stop();
    return used==size_t(contentLength)?classifyResponse(status,body,used,job.deviceCode,job.sample):Outcome::Retry;
}
}
