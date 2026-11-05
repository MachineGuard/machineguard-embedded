#include "Node.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cinttypes>

namespace mg {
namespace {
bool identifier(const char* value,size_t maximum) {
    if(!value || !value[0])return false;
    for(size_t i=0;value[i];++i) {
        if(i>=maximum)return false;
        const char c=value[i];
        if(!((c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9') || c=='_' || c=='-'))return false;
        if(i==0 && (c=='_' || c=='-'))return false;
    }
    return true;
}
bool interval(uint32_t value) {return value>0 && value<=0x7fffffffU;}
bool terminal(Outcome o) {return o==Outcome::Discarded || o==Outcome::Quarantined || o==Outcome::EdgeOverflow
    || o==Outcome::InvalidSample || o==Outcome::Conflict;}
}
bool elapsed(uint32_t now,uint32_t since,uint32_t duration) {return uint32_t(now-since)>=duration;}
bool nextBootCounter(uint64_t previous,uint64_t& next) {
    if(previous==UINT64_MAX)return false;
    next=previous+1;return true;
}
bool makeBootId(uint64_t mac,uint64_t counter,const uint8_t entropy[16],char (&out)[65]) {
    out[0]=0;
    if(!counter || mac>0xffffffffffffULL || !entropy)return false;
    int n=std::snprintf(out,sizeof out,"%012" PRIx64 "-%016" PRIx64 "-",mac,counter);
    if(n!=30)return false;
    constexpr char hex[]="0123456789abcdef";
    for(size_t i=0;i<16;++i) {out[30+i*2]=hex[entropy[i]>>4];out[31+i*2]=hex[entropy[i]&15];}
    out[62]=0;return true;
}
bool validSettings(const Settings& s) {
    return identifier(s.deviceCode,64) && s.capacity>=1 && s.capacity<=MaxSamples
        && s.sensorMinimumMs>=2100 && interval(s.sensorMinimumMs) && s.sampleMs>=s.sensorMinimumMs && interval(s.sampleMs)
        && s.debounceMs>=10 && s.debounceMs<=200 && s.holdMs>=5000 && interval(s.holdMs)
        && interval(s.pairingMs) && interval(s.retryMs) && s.retryMaximumMs>=s.retryMs && interval(s.retryMaximumMs)
        && s.localTemperatureMinimum>=-400 && s.localTemperatureMaximum<=800 && s.localTemperatureMinimum<s.localTemperatureMaximum
        && s.localHumidityMinimum>=0 && s.localHumidityMaximum<=1000 && s.localHumidityMinimum<s.localHumidityMaximum;
}
bool formatTimestamp(int64_t epochMs,char (&out)[25]) {
    out[0]=0;
    if(epochMs<MinimumEpochMs || epochMs>4102444799999LL)return false;
    int64_t seconds=epochMs/1000,days=seconds/86400;
    int year=1970;
    auto leap=[](int y){return y%4==0 && (y%100!=0 || y%400==0);};
    while(days>=(leap(year)?366:365)) {days-=leap(year)?366:365;++year;}
    const int lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};int month=0;
    while(days>=lengths[month]+(month==1 && leap(year)?1:0)) {days-=lengths[month]+(month==1 && leap(year)?1:0);++month;}
    const int day=int(days)+1,hour=int(seconds%86400/3600),minute=int(seconds%3600/60),second=int(seconds%60),ms=int(epochMs%1000);
    return std::snprintf(out,sizeof out,"%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",year,month+1,day,hour,minute,second,ms)==24;
}
bool encodePayload(const char* deviceCode,const Sample& sample,char* out,size_t cap) {
    if(!out || !cap)return false;
    out[0]=0;
    if(!identifier(deviceCode,64) || !identifier(sample.sampleId,85)
       || sample.temperatureTenths < -400 || sample.temperatureTenths>800 || sample.humidityTenths<0 || sample.humidityTenths>1000)return false;
    char timestamp[25]{};
    if(sample.hasCapturedAt && (!formatTimestamp(sample.epochMs,timestamp) || std::strcmp(timestamp,sample.capturedAt)!=0))return false;
    const int t=sample.temperatureTenths<0?-int(sample.temperatureTenths):int(sample.temperatureTenths);
    int n=std::snprintf(out,cap,"{\"deviceCode\":\"%s\",\"sampleId\":\"%s\",\"temperature\":%s%d.%d,\"humidity\":%d.%d%s%s%s}",
        deviceCode,sample.sampleId,sample.temperatureTenths<0?"-":"",t/10,t%10,
        int(sample.humidityTenths)/10,int(sample.humidityTenths)%10,
        sample.hasCapturedAt?",\"capturedAt\":\"":"",sample.hasCapturedAt?sample.capturedAt:"",sample.hasCapturedAt?"\"":"");
    if(n<0 || size_t(n)>=cap) {out[0]=0;return false;}
    return true;
}
Node::Node(const Settings& settings,const char* bootId,uint32_t now,uint64_t sequence,bool known,uint32_t lost)
    :settings_(settings),nextSequence_(sequence),lastSensor_(now),backoff_(settings.retryMs) {
    valid_=validSettings(settings) && identifier(bootId,64) && lost<=MaxSamples;
    if(valid_)std::strncpy(bootId_,bootId,sizeof bootId_-1);
    stats_.resetLossKnown=known;stats_.resetPendingDiscarded=known?lost:0;
}
void Node::tick(uint32_t now,bool pressed,bool networkConnected) {
    network_=networkConnected;
    if(pressed!=rawButton_) {rawButton_=pressed;rawChanged_=now;}
    if(button_!=rawButton_ && elapsed(now,rawChanged_,settings_.debounceMs)) {
        button_=rawButton_;
        if(button_) {pressedAt_=now;longSent_=false;}
        else if(!longSent_) {manual_=true;++stats_.shortPresses;if(alarm_)muted_=true;}
    }
    if(button_ && !longSent_ && elapsed(now,pressedAt_,settings_.holdMs)) {
        pairing_=true;pairStarted_=now;longSent_=true;++stats_.longPresses;
    }
    if(pairing_ && elapsed(now,pairStarted_,settings_.pairingMs))pairing_=false;
}
bool Node::beginRead(uint32_t now) {
    if(!valid_ || reading_ || stats_.sequenceExhausted || !elapsed(now,lastSensor_,settings_.sensorMinimumMs))return false;
    if(!manual_ && stats_.acquired+stats_.sensorFailures>0 && !elapsed(now,lastSensor_,settings_.sampleMs))return false;
    reading_=true;return true;
}
bool Node::finishRead(uint32_t now,float temperature,float humidity,CaptureTime time) {
    if(!reading_ || !valid_)return false;
    reading_=false;manual_=false;lastSensor_=now;
    if(!std::isfinite(temperature) || !std::isfinite(humidity) || temperature < -40 || temperature>80 || humidity<0 || humidity>100
       || std::fabs(temperature*10-std::round(temperature*10))>0.001F || std::fabs(humidity*10-std::round(humidity*10))>0.001F) {
        ++stats_.sensorFailures;sensorOk_=false;return false;
    }
    Sample sample{};sample.temperatureTenths=int16_t(std::lround(temperature*10));sample.humidityTenths=int16_t(std::lround(humidity*10));
    std::snprintf(sample.sampleId,sizeof sample.sampleId,"%s-%" PRIu64,bootId_,nextSequence_);
    if(nextSequence_==UINT64_MAX)stats_.sequenceExhausted=true;else ++nextSequence_;
    if(time.valid && formatTimestamp(time.epochMs,sample.capturedAt)) {
        if(time.epochMs>lastEpoch_) {sample.hasCapturedAt=true;sample.epochMs=time.epochMs;lastEpoch_=time.epochMs;}
        else {sample.capturedAt[0]=0;++stats_.clockRegressions;}
    }
    ++stats_.acquired;sensorOk_=true;
    const bool unsafe=sample.temperatureTenths<settings_.localTemperatureMinimum || sample.temperatureTenths>settings_.localTemperatureMaximum
        || sample.humidityTenths<settings_.localHumidityMinimum || sample.humidityTenths>settings_.localHumidityMaximum;
    if(!unsafe || !alarm_)muted_=false;
    alarm_=unsafe;
    if(count_==settings_.capacity) {
        if(inFlight_ && count_==1) {++stats_.ramOverflow;return false;}
        remove(inFlight_?1:0);++stats_.ramOverflow;
    }
    samples_[count_++]=sample;return true;
}
bool Node::beginSend(uint32_t now,Sample& copy,uint32_t& ticket) {
    if(!valid_ || inFlight_ || !count_ || (retrying_ && !elapsed(now,lastRetry_,backoff_)))return false;
    inFlight_=true;++ticket_;if(ticket_==0)++ticket_;
    ticket=ticket_;copy=samples_[0];return true;
}
bool Node::complete(uint32_t now,uint32_t ticket,Outcome outcome) {
    if(!inFlight_ || ticket!=ticket_) {++stats_.staleCompletions;return false;}
    inFlight_=false;
    if(static_cast<uint8_t>(outcome)>static_cast<uint8_t>(Outcome::Conflict))outcome=Outcome::InvalidReceipt;
    if(outcome==Outcome::Retry || outcome==Outcome::AuthRetry || outcome==Outcome::InvalidReceipt) {
        if(outcome==Outcome::AuthRetry)++stats_.authFailures;
        if(outcome==Outcome::InvalidReceipt)++stats_.invalidReceipts;
        ++stats_.retries;
        backoff_=retrying_?uint32_t(backoff_>settings_.retryMaximumMs/2?settings_.retryMaximumMs:backoff_*2):settings_.retryMs;
        retrying_=true;lastRetry_=now;return true;
    }
    if(terminal(outcome))retainRejection(samples_[0],outcome);
    if(outcome==Outcome::LocalPending)++stats_.localPending;
    else if(outcome==Outcome::Confirmed)++stats_.confirmed;
    else if(outcome==Outcome::Discarded)++stats_.discarded;
    else if(outcome==Outcome::Quarantined)++stats_.quarantined;
    else if(outcome==Outcome::EdgeOverflow)++stats_.edgeOverflow;
    else ++stats_.rejected;
    remove(0);retrying_=false;backoff_=settings_.retryMs;return true;
}
void Node::remove(size_t index) {
    if(index>=count_)return;
    for(size_t i=index;i+1<count_;++i)samples_[i]=samples_[i+1];
    --count_;
}
void Node::retainRejection(const Sample& sample,Outcome reason) {
    if(rejectionCount_>=MaxRejected)++stats_.rejectionEvidenceEvicted;
    rejected_[rejectionCount_%MaxRejected]={sample,reason};++rejectionCount_;
}
Lights Node::lights(uint32_t now) const {
    Lights result{};
    result.red=!sensorOk_;
    result.blue=pairing_ && (uint32_t(now-pairStarted_)/150)%2==0;
    const bool degraded=!network_ || retrying_ || stats_.ramOverflow || stats_.edgeOverflow || stats_.quarantined || stats_.rejected;
    result.amber=degraded && (now/500)%2==0;
    result.green=valid_ && network_ && sensorOk_ && !retrying_ && !pairing_;
    result.buzzer=settings_.buzzerEnabled && alarm_ && !muted_;return result;
}
}
