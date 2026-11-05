#include "Node.h"
#include "Ack.h"
#include "HttpHeaders.h"
#include "BootMemory.h"
#include "NetworkPolicy.h"
#include <ArduinoJson.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#include <functional>

static int failures=0;
#define CHECK(expression) do { if(!(expression)) {std::printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#expression);++failures;return;} } while(false)
using namespace mg;
static Settings settings() {Settings s;s.buzzerEnabled=true;return s;}
static void acquire(Node& node,uint32_t time,float t=5,float h=50,CaptureTime clock={}) {
    if(node.beginRead(time))node.finishRead(time,t,h,clock);
}
static void identity() {
    uint8_t bytes[16]{};char a[65]{},b[65]{};uint64_t next=0;
    CHECK(nextBootCounter(0,next));CHECK(next==1);
    CHECK(makeBootId(0x112233445566ULL,1,bytes,a));CHECK(makeBootId(0x112233445566ULL,2,bytes,b));
    CHECK(std::strcmp(a,b)!=0);CHECK(std::strlen(a)<=64);
    CHECK(!nextBootCounter(UINT64_MAX,next));
}
static void first_sample() {
    Node n(settings(),"bootA",0);CHECK(n.valid());CHECK(!n.beginRead(2099));
    CHECK(n.beginRead(2100));CHECK(n.finishRead(2100,5,50,{}));CHECK(n.pending()==1);
    CHECK(std::strcmp(n.oldest()->sampleId,"bootA-0")==0);CHECK(!n.oldest()->hasCapturedAt);
}
static void frozen_retry() {
    Node n(settings(),"bootA",0);acquire(n,2100);
    Sample a{};uint32_t ticket=0;CHECK(n.beginSend(2100,a,ticket));
    CHECK(n.complete(2200,ticket,Outcome::Retry));
    acquire(n,12100,6,51,{true,MinimumEpochMs+100000});
    Sample b{};CHECK(n.beginSend(12100,b,ticket));CHECK(std::strcmp(a.sampleId,b.sampleId)==0);
    CHECK(!b.hasCapturedAt);CHECK(b.temperatureTenths==a.temperatureTenths);
    char before[512]{},after[512]{};CHECK(encodePayload("ESP32-01",a,before,sizeof before));
    CHECK(encodePayload("ESP32-01",b,after,sizeof after));CHECK(std::strcmp(before,after)==0);
}
static void local_is_not_core() {
    Node n(settings(),"bootA",0);acquire(n,2100);
    Sample a{};uint32_t ticket=0;CHECK(n.beginSend(2100,a,ticket));CHECK(n.complete(2200,ticket,Outcome::LocalPending));
    CHECK(n.pending()==0);CHECK(n.stats().localPending==1);CHECK(n.stats().confirmed==0);
}
static void no_fake_sensor() {
    Node n(settings(),"bootA",0);acquire(n,2100,std::numeric_limits<float>::quiet_NaN());
    CHECK(n.pending()==0);CHECK(n.stats().sensorFailures==1);CHECK(n.lights(2100).red);
}

static Sample sent() {Node n(settings(),"bootA",0);acquire(n,2100);return *n.oldest();}
static std::string receipt(const Sample& sample,const char* status="CALIBRATED",const char* delivery="PENDING") {
    StaticJsonDocument<1024> doc;doc["id"]=1;doc["deviceCode"]="ESP32-01";doc["sampleId"]=sample.sampleId;
    doc["rawTemperature"]=sample.temperatureTenths/10.0;doc["rawHumidity"]=sample.humidityTenths/10.0;
    doc["capturedAt"]=sample.hasCapturedAt?sample.capturedAt:"2026-11-05T12:00:00Z";
    doc["persistence"]="LOCAL_COMMITTED";doc["status"]=status;doc["delivery"]=delivery;
    std::string out;serializeJson(doc,out);return out;
}
static Outcome classify(const Sample& sample,const std::string& body,int status=201) {
    return classifyResponse(status,body.data(),body.size(),"ESP32-01",sample);
}
static void receipt_local() {auto sample=sent();CHECK(classify(sample,receipt(sample))==Outcome::LocalPending);}
static void receipt_confirmed() {auto sample=sent();CHECK(classify(sample,receipt(sample,"SYNCED","CONFIRMED"))==Outcome::Confirmed);}
static void receipt_terminals() {
    auto sample=sent();CHECK(classify(sample,receipt(sample,"DISCARDED","NOT_QUEUED"),202)==Outcome::Discarded);
    CHECK(classify(sample,receipt(sample,"QUARANTINED","QUARANTINED"))==Outcome::Quarantined);
    CHECK(classify(sample,receipt(sample,"DROPPED_OVERFLOW","NOT_QUEUED"))==Outcome::EdgeOverflow);
    CHECK(classify(sample,"{\"error\":\"Invalid capture\"}",400)==Outcome::InvalidSample);
    CHECK(classify(sample,"{\"error\":\"sampleId conflict\"}",409)==Outcome::Conflict);
}
static void receipt_no_evidence() {
    auto sample=sent();CHECK(classify(sample,"not JSON")==Outcome::InvalidReceipt);
    CHECK(classify(sample,receipt(sample,"SYNCED","PENDING"))==Outcome::InvalidReceipt);
    auto bad=receipt(sample);bad.replace(bad.find("bootA-0"),7,"other-0");CHECK(classify(sample,bad)==Outcome::InvalidReceipt);
    CHECK(classify(sample,receipt(sample)+"garbage")==Outcome::InvalidReceipt);
}
static void transient_errors() {
    auto sample=sent();CHECK(classify(sample,receipt(sample),401)==Outcome::AuthRetry);
    CHECK(classify(sample,receipt(sample),403)==Outcome::AuthRetry);
    CHECK(classify(sample,receipt(sample),503)==Outcome::Retry);
    CHECK(classify(sample,"",-1)==Outcome::Retry);
    CHECK(classify(sample,"<html>bad</html>",400)==Outcome::InvalidReceipt);
}

static void reset_losses() {
    const auto retained=checkpoint(4,13);uint32_t discarded=0;
    CHECK(previousPending(retained,true,4,discarded));CHECK(discarded==13);
    CHECK(!previousPending(retained,false,4,discarded));CHECK(!previousPending(retained,true,5,discarded));
    auto bad=retained;bad.pending=14;CHECK(!previousPending(bad,true,4,discarded));
    Node restarted(settings(),"bootB",0,0,true,13);CHECK(restarted.pending()==0);CHECK(restarted.stats().resetPendingDiscarded==13);
    Node cold(settings(),"bootC",0);CHECK(!cold.stats().resetLossKnown);
}
static void secure_network() {
    std::string key=std::string("mgn_")+std::string(43,'x');NetworkSettings config;
    config.enabled=true;config.host="edge.bank.example";config.ssid="private-bank";config.password="fictitious-test-password";
    config.credential=key.c_str();config.caCertificate="-----BEGIN CERTIFICATE-----\nFICTITIOUS\n-----END CERTIFICATE-----";
    CHECK(validNetwork(config));config.caCertificate="";CHECK(!validNetwork(config));
    config.tls=false;CHECK(!validNetwork(config));config.dedicatedBankHttp=true;CHECK(!validNetwork(config));
    config.host="192.168.4.2";CHECK(validNetwork(config));config.host="203.0.113.1";CHECK(!validNetwork(config));
    config.host="192.168.4.2";config.password="";CHECK(!validNetwork(config));
}
static void maximum_queue_and_inflight_ownership() {
    Node n(settings(),"bootA",0);acquire(n,2100);Sample snapshot{};uint32_t ticket=0;CHECK(n.beginSend(2100,snapshot,ticket));
    for(uint32_t i=1;i<=60;++i)acquire(n,2100+i*10000);
    CHECK(n.pending()==60);CHECK(n.stats().ramOverflow==1);CHECK(std::strcmp(n.oldest()->sampleId,"bootA-0")==0);
    CHECK(std::strcmp(snapshot.sampleId,"bootA-0")==0);CHECK(n.complete(602101,ticket,Outcome::Confirmed));
    CHECK(std::strcmp(n.oldest()->sampleId,"bootA-2")==0);CHECK(n.stats().confirmed==1);
}
static void terminal_unblocks_queue() {
    Node n(settings(),"bootA",0);acquire(n,2100,25);acquire(n,12100,5);Sample s{};uint32_t ticket=0;
    CHECK(n.beginSend(12100,s,ticket));CHECK(n.complete(12200,ticket,Outcome::InvalidSample));
    CHECK(n.pending()==1);CHECK(n.stats().rejected==1);CHECK(n.lastRejected());CHECK(n.lastRejected()->sample.temperatureTenths==250);
    CHECK(n.beginSend(12200,s,ticket));CHECK(std::strcmp(s.sampleId,"bootA-1")==0);CHECK(n.complete(12300,ticket,Outcome::Confirmed));CHECK(n.pending()==0);
}
static void backoff_and_auth_retain_identity() {
    auto cfg=settings();cfg.retryMaximumMs=4000;Node n(cfg,"bootA",0);acquire(n,2100);Sample s{};uint32_t ticket=0;
    CHECK(n.beginSend(2100,s,ticket));CHECK(n.complete(2200,ticket,Outcome::AuthRetry));CHECK(!n.beginSend(3199,s,ticket));
    CHECK(n.beginSend(3200,s,ticket));CHECK(n.complete(3300,ticket,Outcome::Retry));CHECK(!n.beginSend(5299,s,ticket));
    CHECK(n.beginSend(5300,s,ticket));CHECK(std::strcmp(s.sampleId,"bootA-0")==0);CHECK(n.complete(5400,ticket,Outcome::Retry));
    CHECK(!n.beginSend(9399,s,ticket));CHECK(n.beginSend(9400,s,ticket));CHECK(n.stats().authFailures==1);CHECK(n.pending()==1);
}
static void ntp_and_rollback() {
    Node n(settings(),"bootA",0);acquire(n,2100,5,50,{true,MinimumEpochMs+50123});
    CHECK(n.oldest()->hasCapturedAt);CHECK(std::strcmp(n.oldest()->capturedAt,"2026-01-01T00:00:50.123Z")==0);
    acquire(n,12100,6,51,{true,MinimumEpochMs+50123});Sample first{};uint32_t ticket=0;
    CHECK(n.beginSend(12100,first,ticket));CHECK(n.complete(12101,ticket,Outcome::LocalPending));
    CHECK(!n.oldest()->hasCapturedAt);CHECK(n.stats().clockRegressions==1);
}
static void debounce_short_cooldown_and_mute() {
    Node n(settings(),"bootA",0);acquire(n,2100,25);CHECK(n.lights(2100).buzzer);
    n.tick(2200,true,true);n.tick(2210,false,true);n.tick(2220,true,true);n.tick(2230,false,true);
    CHECK(n.lights(2230).buzzer);n.tick(2300,true,true);n.tick(2340,true,true);n.tick(2400,false,true);n.tick(2440,false,true);
    CHECK(!n.lights(2440).buzzer);CHECK(!n.beginRead(4199));CHECK(n.beginRead(4200));CHECK(n.finishRead(4200,25,50,{}));
    CHECK(!n.lights(4200).buzzer);CHECK(!n.beginRead(4300));
}
static void long_press_and_millis_wrap() {
    const uint32_t start=UINT32_MAX-1000;Node n(settings(),"bootA",start);
    n.tick(start,true,false);n.tick(start+40,true,false);n.tick(start+5039,true,false);CHECK(!n.pairing());
    n.tick(start+5040,true,false);CHECK(n.pairing());n.tick(start+5100,false,false);n.tick(start+5140,false,false);
    CHECK(n.beginRead(start+5140));CHECK(n.finishRead(start+5140,5,50,{}));
    CHECK(!n.beginRead(start+7240));CHECK(n.beginRead(start+15140));CHECK(n.finishRead(start+15140,5,50,{}));
}
static void slow_io_keeps_sampling_and_button() {
    Node n(settings(),"bootA",0);acquire(n,2100);Sample s{};uint32_t ticket=0;CHECK(n.beginSend(2100,s,ticket));
    for(uint32_t now=2101;now<65000;now+=10) {
        const bool pressed=now>=5000 && now<10500;n.tick(now,pressed,false);acquire(n,now);
    }
    CHECK(n.pending()>=6);CHECK(n.stats().acquired>=6);CHECK(n.busy());CHECK(!n.beginSend(65000,s,ticket));
    CHECK(n.complete(65001,ticket,Outcome::InvalidReceipt));CHECK(n.pending()>=6);CHECK(n.stats().invalidReceipts==1);
}
static void sequence_exhaustion() {
    Node n(settings(),"bootA",0,UINT64_MAX);acquire(n,2100);
    CHECK(n.stats().sequenceExhausted);CHECK(std::strcmp(n.oldest()->sampleId,"bootA-18446744073709551615")==0);
    CHECK(!n.beginRead(12100));Sample s{};uint32_t ticket=0;CHECK(n.beginSend(12100,s,ticket));CHECK(n.complete(12101,ticket,Outcome::LocalPending));
}
static void payload_limits_and_negative_tenths() {
    Node n(settings(),"bootA",0);acquire(n,2100,-.1F,50);char output[512]{};CHECK(encodePayload("ESP32-01",*n.oldest(),output,sizeof output));
    CHECK(std::strstr(output,"\"temperature\":-0.1"));StaticJsonDocument<1024> doc;CHECK(!deserializeJson(doc,output));
    CHECK(!doc.containsKey("capturedAt"));CHECK(!encodePayload("bad\r\n",*n.oldest(),output,sizeof output));
    char tiny[8]{};CHECK(!encodePayload("ESP32-01",*n.oldest(),tiny,sizeof tiny));CHECK(tiny[0]==0);
}
static void strict_receipt_types_and_bounds() {
    auto sample=sent();StaticJsonDocument<2048> doc;CHECK(!deserializeJson(doc,receipt(sample)));doc["id"]=true;
    std::string out;serializeJson(doc,out);CHECK(classify(sample,out)==Outcome::InvalidReceipt);
    CHECK(!deserializeJson(doc,receipt(sample)));doc["rawTemperature"]="5";out.clear();serializeJson(doc,out);CHECK(classify(sample,out)==Outcome::InvalidReceipt);
    CHECK(classify(sample,std::string(1537,' '))==Outcome::InvalidReceipt);
    auto bad=receipt(sample);bad.insert(bad.size()-1,",\"bad\":\""+std::string(1,static_cast<char>(-1))+"\"");CHECK(classify(sample,bad)==Outcome::InvalidReceipt);
}
static void timestamp_receipt_equivalence() {
    Node n(settings(),"bootA",0);acquire(n,2100,5,50,{true,MinimumEpochMs});auto sample=*n.oldest();
    auto body=receipt(sample);CHECK(classify(sample,body)==Outcome::LocalPending);
    body.replace(body.find("00:00:00.000Z"),13,"00:00:00Z");CHECK(classify(sample,body)==Outcome::LocalPending);
}

static void overflow_still_updates_local_alarm() {
    auto cfg=settings();cfg.capacity=1;Node n(cfg,"bootA",0);acquire(n,2100);Sample s{};uint32_t ticket=0;
    CHECK(n.beginSend(2100,s,ticket));acquire(n,12100,25);
    CHECK(n.stats().ramOverflow==1);CHECK(n.pending()==1);CHECK(n.lights(12100).buzzer);
}
static void unknown_completion_never_releases_sample() {
    Node n(settings(),"bootA",0);acquire(n,2100);Sample s{};uint32_t ticket=0;CHECK(n.beginSend(2100,s,ticket));
    CHECK(n.complete(2200,ticket,static_cast<Outcome>(255)));CHECK(n.pending()==1);CHECK(n.stats().invalidReceipts==1);
}
static void timestamps_and_invalid_settings() {
    char time[25]{};CHECK(formatTimestamp(1835395200123LL,time));CHECK(std::strcmp(time,"2028-02-29T00:00:00.123Z")==0);
    CHECK(formatTimestamp(4102444799999LL,time));CHECK(std::strcmp(time,"2099-12-31T23:59:59.999Z")==0);
    CHECK(!formatTimestamp(MinimumEpochMs-1,time));CHECK(!formatTimestamp(4102444800000LL,time));
    auto cfg=settings();cfg.sampleMs=1000;Node bad(cfg,"bootA",0);CHECK(!bad.valid());CHECK(!bad.beginRead(10000));
}
static void rejected_evidence_is_bounded_and_preserved() {
    Node n(settings(),"bootA",0);
    for(uint32_t i=0;i<10;++i) {
        acquire(n,2100+i*10000,25);Sample sample{};uint32_t ticket=0;CHECK(n.beginSend(2100+i*10000,sample,ticket));
        CHECK(n.complete(2200+i*10000,ticket,Outcome::Quarantined));CHECK(n.lastRejected()->sample.temperatureTenths==250);
    }
    CHECK(n.stats().quarantined==10);CHECK(n.stats().rejectionEvidenceEvicted==2);CHECK(n.stats().confirmed==0);
}

static void largest_edge_identity_serializes() {
    std::string boot(64,'a');Node n(settings(),boot.c_str(),0,UINT64_MAX);acquire(n,2100);
    CHECK(std::strlen(n.oldest()->sampleId)==85);char payload[512]{};
    CHECK(encodePayload("ESP32-01",*n.oldest(),payload,sizeof payload));
}

static void http_headers_boundaries() {
    HttpHeaders result{};const std::string good="HTTP/1.0 201 Created\r\nContent-Type: application/json\r\nContent-Length: 100\r\n\r\n";
    CHECK(parseHttpHeaders(good.data(),good.size(),result));CHECK(result.status==201);CHECK(result.bodyLength==100);
    const std::string bad[]={"HTTP/1.0 201 OK\r\nContent-Length: 10x\r\n\r\n", "HTTP/1.0 201 OK\r\nContent-Length: 10 garbage\r\n\r\n",
        "HTTP/1.0 201 OK\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\n", "HTTP/1.1 201 OK\r\nTransfer-Encoding: chunked\r\nContent-Length: 10\r\n\r\n",
        "HTTP/1.0 201 OK\r\nContent-Length: 99999999999999\r\n\r\n", "HTTP/1.0 201x\r\nContent-Length: 10\r\n\r\n"};
    for(const auto& header:bad)CHECK(!parseHttpHeaders(header.data(),header.size(),result));
}
static void duplicate_json_cannot_confirm() {
    auto sample=sent();auto good=receipt(sample);good.insert(good.size()-1,",\"sampleId\":\"bootA-0\"");
    CHECK(classify(sample,good)==Outcome::InvalidReceipt);
}

static void retries_and_sampling_survive_millis_wrap() {
    const uint32_t start=UINT32_MAX-3000;Node n(settings(),"bootA",start);acquire(n,start+2100);
    Sample sample{};uint32_t ticket=0;CHECK(n.beginSend(start+2100,sample,ticket));CHECK(n.complete(start+2200,ticket,Outcome::Retry));
    CHECK(!n.beginSend(start+3199,sample,ticket));CHECK(n.beginSend(start+3200,sample,ticket));
    CHECK(!n.complete(start+3250,ticket-1,Outcome::Confirmed));CHECK(n.pending()==1);
    CHECK(n.complete(start+3300,ticket,Outcome::Confirmed));CHECK(n.pending()==0);CHECK(n.stats().staleCompletions==1);
}
static void all_terminal_statuses_are_not_core_success() {
    for(const auto outcome:{Outcome::Discarded,Outcome::Quarantined,Outcome::EdgeOverflow,Outcome::InvalidSample,Outcome::Conflict}) {
        Node n(settings(),"bootA",0);acquire(n,2100);Sample sample{};uint32_t ticket=0;CHECK(n.beginSend(2100,sample,ticket));
        CHECK(n.complete(2200,ticket,outcome));CHECK(n.pending()==0);CHECK(n.stats().confirmed==0);CHECK(n.stats().localPending==0);
        CHECK(n.lastRejected()->reason==outcome);CHECK(std::strcmp(n.lastRejected()->sample.sampleId,sample.sampleId)==0);
    }
}
static void sampling_never_reuses_cached_frame() {
    Node n(settings(),"bootA",0);CHECK(n.beginRead(2100));CHECK(!n.beginRead(2101));
    CHECK(n.finishRead(2110,5,50,{}));CHECK(!n.finishRead(2111,5,50,{}));CHECK(n.pending()==1);
    CHECK(!n.beginRead(12109));CHECK(n.beginRead(12110));CHECK(n.finishRead(12120,6,51,{}));CHECK(n.pending()==2);
}
int main() {
    const std::vector<std::pair<const char*,std::function<void()>>> tests={
        {"boot identity",identity},{"first sample",first_sample},{"frozen retry",frozen_retry},
        {"local receipt",local_is_not_core},{"sensor failure",no_fake_sensor},{"receipt local",receipt_local},{"receipt confirmed",receipt_confirmed},
        {"terminal receipts",receipt_terminals},{"malformed receipts",receipt_no_evidence},{"transient errors",transient_errors},{"reset losses",reset_losses},{"secure network",secure_network},
        {"bounded queue ownership",maximum_queue_and_inflight_ownership},{"terminal isolation",terminal_unblocks_queue},
        {"backoff auth",backoff_and_auth_retain_identity},{"NTP rollback",ntp_and_rollback},
        {"debounce and mute",debounce_short_cooldown_and_mute},{"button wrap",long_press_and_millis_wrap},
        {"slow IO",slow_io_keeps_sampling_and_button},{"sequence exhaustion",sequence_exhaustion},
        {"payload limits",payload_limits_and_negative_tenths},{"strict receipt",strict_receipt_types_and_bounds},
        {"capture equivalence",timestamp_receipt_equivalence},{"overflow local alarm",overflow_still_updates_local_alarm},
        {"unknown completion",unknown_completion_never_releases_sample},{"calendar/settings",timestamps_and_invalid_settings},
        {"bounded rejection evidence",rejected_evidence_is_bounded_and_preserved},{"largest identity",largest_edge_identity_serializes},{"HTTP framing",http_headers_boundaries},{"duplicate JSON",duplicate_json_cannot_confirm},{"retry wrap",retries_and_sampling_survive_millis_wrap},
        {"all terminal outcomes",all_terminal_statuses_are_not_core_success},{"no sensor cache",sampling_never_reuses_cached_frame}};
    for(const auto& test:tests) {int before=failures;test.second();std::printf("%s %s\n",failures==before?"PASS":"FAIL",test.first);}
    std::printf("Sample bytes=%zu Node bytes=%zu\n",sizeof(Sample),sizeof(Node));
    std::printf("%zu cases, %d failures\n",tests.size(),failures);return failures?1:0;
}
