#include "Ack.h"
#include <ArduinoJson.h>
#include <cmath>
#include <cstring>
namespace mg {
namespace {
bool utf8(const char* text,size_t length) {
    for(size_t i=0;i<length;) {
        const auto c=static_cast<unsigned char>(text[i++]);
        if(c<0x80)continue;
        int more=0;unsigned code=0,minimum=0;
        if(c>=0xc2 && c<=0xdf) {more=1;code=c&31;minimum=0x80;}
        else if(c>=0xe0 && c<=0xef) {more=2;code=c&15;minimum=0x800;}
        else if(c>=0xf0 && c<=0xf4) {more=3;code=c&7;minimum=0x10000;}
        else return false;
        if(i+size_t(more)>length)return false;
        while(more--) {const auto next=static_cast<unsigned char>(text[i++]);if((next&0xc0)!=0x80)return false;code=(code<<6)|(next&63);}
        if(code<minimum || code>0x10ffff || (code>=0xd800 && code<=0xdfff))return false;
    }
    return true;
}
bool space(char c) {return c==' ' || c=='\t' || c=='\r' || c=='\n';}
bool frame(const char* data,size_t length) {
    if(!data || !length || length>ResponseBytes || !utf8(data,length))return false;
    size_t start=0;while(start<length && space(data[start]))++start;
    if(start==length || data[start]!='{')return false;
    unsigned depth=0;bool quoted=false,escape=false;
    char keys[24][65]{};size_t keyCount=0;
    for(size_t i=start;i<length;++i) {
        const char c=data[i];
        if(quoted) {
            if(static_cast<unsigned char>(c)<32)return false;
            if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='"')quoted=false;
        } else {
            if(c=='"') {
                // Fixed Edge keys are plain ASCII. Reject duplicate/escaped top-level keys
                // before ArduinoJson would collapse them into a single property.
                size_t previous=i;while(previous>start && space(data[previous-1]))--previous;
                if(depth==1 && (data[previous-1]=='{' || data[previous-1]==',')) {
                    size_t end=i+1;
                    while(end<length && data[end]!='"') {
                        if(data[end]=='\\' || static_cast<unsigned char>(data[end])<32 || end-i>64)return false;++end;
                    }
                    if(end==length || keyCount>=24)return false;
                    const size_t size=end-i-1;
                    for(size_t k=0;k<keyCount;++k)if(std::strlen(keys[k])==size && std::memcmp(keys[k],data+i+1,size)==0)return false;
                    std::memcpy(keys[keyCount++],data+i+1,size);
                }
                quoted=true;
            }
            else if(c=='{' || c=='[') {if(++depth>4)return false;}
            else if(c=='}' || c==']') {
                if(!depth)return false;
                if(--depth==0) {for(size_t j=i+1;j<length;++j)if(!space(data[j]))return false;return c=='}';}
            }
        }
    }
    return false;
}
bool equal(JsonVariantConst value,const char* expected) {
    if(!value.is<const char*>() || !expected)return false;
    const JsonString actual=value.as<JsonString>();const size_t size=std::strlen(expected);
    return actual.size()==size && std::memcmp(actual.c_str(),expected,size)==0;
}
bool rawValue(JsonVariantConst value,int16_t tenths) {
    return !value.is<bool>() && value.is<double>() && std::isfinite(value.as<double>())
        && std::fabs(value.as<double>()-tenths/10.0)<0.0001;
}
int digits(const char* text,size_t position,size_t count) {
    int value=0;for(size_t i=position;i<position+count;++i) {if(text[i]<'0' || text[i]>'9')return -1;value=value*10+text[i]-'0';}return value;
}
bool capture(JsonVariantConst value,const Sample& sent) {
    if(!value.is<const char*>())return false;
    const auto text=value.as<JsonString>();const size_t length=text.size();const char* p=text.c_str();
    if(length!=20 && length!=24 && length!=27)return false;
    if(p[4]!='-' || p[7]!='-' || p[10]!='T' || p[13]!=':' || p[16]!=':' || p[length-1]!='Z')return false;
    const int y=digits(p,0,4),m=digits(p,5,2),d=digits(p,8,2),h=digits(p,11,2),minute=digits(p,14,2),second=digits(p,17,2);
    const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(y<2026 || y>2099 || m<1 || m>12 || d<1 || d>days[m-1]+(m==2 && y%4==0?1:0)
       || h<0 || h>23 || minute<0 || minute>59 || second<0 || second>59)return false;
    int micros=0;
    if(length!=20) {
        if(p[19]!='.')return false;
        micros=digits(p,20,length-21);if(micros<0)return false;if(length==24)micros*=1000;
    }
    return !sent.hasCapturedAt || (std::memcmp(p,sent.capturedAt,19)==0 && micros==int(sent.epochMs%1000)*1000);
}
}
Outcome classifyResponse(int http,const char* body,size_t length,const char* device,const Sample& sent) {
    if(http==401 || http==403)return Outcome::AuthRetry;
    if(http<0 || http>=500 || http==404 || http==413 || http==429)return Outcome::Retry;
    if(!frame(body,length))return Outcome::InvalidReceipt;
    StaticJsonDocument<3072> doc;
    if(deserializeJson(doc,body,length,DeserializationOption::NestingLimit(4)) || !doc.is<JsonObject>())return Outcome::InvalidReceipt;
    if(http==400 || http==409) {
        const auto error=doc["error"].as<JsonString>();
        if(!doc["error"].is<const char*>() || !error.size() || error.size()>512)return Outcome::InvalidReceipt;
        if((doc.containsKey("sampleId") && !equal(doc["sampleId"],sent.sampleId))
           || (doc.containsKey("deviceCode") && !equal(doc["deviceCode"],device)))return Outcome::InvalidReceipt;
        return http==400?Outcome::InvalidSample:Outcome::Conflict;
    }
    if(http<200 || http>=300 || !doc["id"].is<int64_t>() || doc["id"].is<bool>() || doc["id"].as<int64_t>()<1
       || !equal(doc["deviceCode"],device) || !equal(doc["sampleId"],sent.sampleId)
       || !equal(doc["persistence"],"LOCAL_COMMITTED") || !rawValue(doc["rawTemperature"],sent.temperatureTenths)
       || !rawValue(doc["rawHumidity"],sent.humidityTenths) || !capture(doc["capturedAt"],sent))return Outcome::InvalidReceipt;
    if(equal(doc["status"],"SYNCED") && equal(doc["delivery"],"CONFIRMED"))return Outcome::Confirmed;
    if(equal(doc["status"],"CALIBRATED") && equal(doc["delivery"],"PENDING"))return Outcome::LocalPending;
    if(equal(doc["status"],"DISCARDED") && equal(doc["delivery"],"NOT_QUEUED"))return Outcome::Discarded;
    if(equal(doc["status"],"QUARANTINED") && equal(doc["delivery"],"QUARANTINED"))return Outcome::Quarantined;
    if(equal(doc["status"],"DROPPED_OVERFLOW") && equal(doc["delivery"],"NOT_QUEUED"))return Outcome::EdgeOverflow;
    return Outcome::InvalidReceipt;
}
}
