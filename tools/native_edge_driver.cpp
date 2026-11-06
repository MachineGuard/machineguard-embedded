// PC contract driver: firmware logic + Windows loopback transport, never an ESP32 simulator.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include <ArduinoJson.h>
#include "Node.h"
#include "Ack.h"
#include "BootMemory.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <future>
#include <string>

namespace {
const char* name(mg::Outcome outcome) {
    switch(outcome) {
        case mg::Outcome::Retry:return "RETRY";case mg::Outcome::AuthRetry:return "AUTH_RETRY";
        case mg::Outcome::InvalidReceipt:return "INVALID_RECEIPT";case mg::Outcome::LocalPending:return "LOCAL_PENDING";
        case mg::Outcome::Confirmed:return "CONFIRMED";case mg::Outcome::Discarded:return "DISCARDED";
        case mg::Outcome::Quarantined:return "QUARANTINED";case mg::Outcome::EdgeOverflow:return "EDGE_OVERFLOW";
        case mg::Outcome::InvalidSample:return "INVALID_SAMPLE";case mg::Outcome::Conflict:return "CONFLICT";
    }
    return "UNKNOWN";
}
struct Handle {HINTERNET value=nullptr;~Handle(){if(value)WinHttpCloseHandle(value);} };
struct Response {int status=-1;std::string body;};
Response post(unsigned port,const std::string& credential,const char* payload) {
    Response response;
    // No proxy, arbitrary host, redirects or TLS bypass. Only test HTTP on 127.0.0.1.
    Handle session{WinHttpOpen(L"MachineGuard native contract test",WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session.value || !WinHttpSetTimeouts(session.value,1000,1000,1000,2000))return response;
    Handle connection{WinHttpConnect(session.value,L"127.0.0.1",INTERNET_PORT(port),0)};
    if(!connection.value)return response;
    Handle request{WinHttpOpenRequest(connection.value,L"POST",L"/api/v1/edge/readings",nullptr,
        WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,0)};
    if(!request.value)return response;
    DWORD disabled=WINHTTP_DISABLE_REDIRECTS;
    if(!WinHttpSetOption(request.value,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof disabled))return response;
    std::wstring headers=L"Content-Type: application/json\r\n";
    if(!credential.empty())headers+=L"X-Node-Credential: "+std::wstring(credential.begin(),credential.end())+L"\r\n";
    const DWORD size=DWORD(std::strlen(payload));
    if(!WinHttpSendRequest(request.value,headers.c_str(),DWORD(headers.size()),const_cast<char*>(payload),size,size,0)
       || !WinHttpReceiveResponse(request.value,nullptr))return response;
    DWORD status=0,statusSize=sizeof status;
    if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,&status,&statusSize,WINHTTP_NO_HEADER_INDEX))return response;
    response.status=int(status);
    char buffer[mg::ResponseBytes+1]{};DWORD read=0;
    do {
        if(!WinHttpReadData(request.value,buffer,sizeof buffer,&read)) {response.body.clear();return response;}
        if(response.body.size()+read>mg::ResponseBytes) {response.body.clear();return response;}
        response.body.append(buffer,read);
    } while(read);
    return response;
}
}
int main() {
    mg::Settings settings;std::unique_ptr<mg::Node> node;std::string credential;unsigned port=0;
    struct Flight { Response response;std::string payload;mg::Sample sample;uint32_t ticket;long long wallMs; };
    std::future<Flight> flight;
    std::string line;
    while(std::getline(std::cin,line)) {
        if(line.size()>2048)return 2;
        StaticJsonDocument<4096> command,output;
        if(deserializeJson(command,line))return 3;
        const std::string action=command["action"]|"";
        if(action=="init") {
            if(node)return 4;
            const char* code=command["deviceCode"]|"";const char* boot=command["bootId"]|"";
            if(std::strlen(code)>64)return 4;std::strcpy(settings.deviceCode,code);
            settings.capacity=command["capacity"]|60;settings.buzzerEnabled=true;
            node=std::make_unique<mg::Node>(settings,boot,command["now"].as<uint32_t>());
            port=command["port"]|0;if(!node->valid() || port<1024 || port>65535)return 4;
            output["initialized"]=true;
        } else if(!node)return 4;
        else if(action=="credential") {
            credential=command["value"]|"";
            if(credential.size()>200 || credential.find_first_of("\r\n")!=std::string::npos)return 4;
            output["configured"]=true; // Never echo the credential or input line.
        } else if(action=="capture") {
            uint32_t now=command["now"].as<uint32_t>();mg::CaptureTime clock{};
            if(command.containsKey("epochMs"))clock={true,command["epochMs"].as<int64_t>()};
            bool started=node->beginRead(now);output["readStarted"]=started;
            output["acquired"]=started && node->finishRead(now,command["temperature"].as<float>(),command["humidity"].as<float>(),clock);
        } else if(action=="tick") {
            node->tick(command["now"].as<uint32_t>(),command["pressed"]|false,command["connected"]|false);
        } else if(action=="send" || action=="sendAsync") {
            mg::Sample sample{};uint32_t ticket=0;const uint32_t now=command["now"].as<uint32_t>();
            bool started=node->beginSend(now,sample,ticket);output["started"]=started;
            if(started) {
                char payload[mg::PayloadBytes]{};if(!mg::encodePayload(settings.deviceCode,sample,payload,sizeof payload))return 5;
                if(action=="sendAsync") {
                    if(flight.valid())return 7;
                    flight=std::async(std::launch::async,[port,key=credential,body=std::string(payload),sample,ticket] {
                        const auto started=std::chrono::steady_clock::now();auto response=post(port,key,body.c_str());
                        return Flight{std::move(response),body,sample,ticket,std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count()};
                    });
                } else {
                const auto begin=std::chrono::steady_clock::now();auto response=post(port,credential,payload);
                auto outcome=mg::classifyResponse(response.status,response.body.data(),response.body.size(),settings.deviceCode,sample);
                if(!node->complete(now,ticket,outcome))return 5;
                output["httpStatus"]=response.status;output["outcome"]=name(outcome);output["payload"]=std::string(payload);
                output["wallMs"]=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count();
                }
            }
        } else if(action=="collect") {
            const bool ready=flight.valid() && flight.wait_for(std::chrono::milliseconds(0))==std::future_status::ready;
            output["ready"]=ready;
            if(ready) {
                auto sent=flight.get();auto outcome=mg::classifyResponse(sent.response.status,sent.response.body.data(),sent.response.body.size(),settings.deviceCode,sent.sample);
                if(!node->complete(command["now"].as<uint32_t>(),sent.ticket,outcome))return 5;
                output["httpStatus"]=sent.response.status;output["outcome"]=name(outcome);output["payload"]=sent.payload;output["wallMs"]=sent.wallMs;
            }
        } else if(action=="state") {
            if(node->oldest()) {char payload[mg::PayloadBytes]{};if(!mg::encodePayload(settings.deviceCode,*node->oldest(),payload,sizeof payload))return 5;output["oldestPayload"]=std::string(payload);}
        } else if(action=="stop")return 0;
        else return 6;
        const auto& stats=node->stats();output["pending"]=node->pending();output["confirmed"]=stats.confirmed;
        output["localPending"]=stats.localPending;output["rejected"]=stats.rejected;
        output["ramOverflow"]=stats.ramOverflow;output["authFailures"]=stats.authFailures;
        output["discarded"]=stats.discarded;output["quarantined"]=stats.quarantined;output["edgeOverflow"]=stats.edgeOverflow;
        output["sensorFailures"]=stats.sensorFailures;output["shortPresses"]=stats.shortPresses;output["longPresses"]=stats.longPresses;
        output["invalidReceipts"]=stats.invalidReceipts;output["clockRegressions"]=stats.clockRegressions;
        std::string encoded;serializeJson(output,encoded);std::cout<<encoded<<std::endl;
    }
    return 0;
}
