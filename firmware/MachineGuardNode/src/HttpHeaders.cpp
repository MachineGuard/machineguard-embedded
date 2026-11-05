#include "HttpHeaders.h"
#include <cstring>
namespace mg {
namespace {
bool equal(const char* data,size_t size,const char* expected) {
    if(size!=std::strlen(expected))return false;
    for(size_t i=0;i<size;++i) {char c=data[i];if(c>='A' && c<='Z')c=char(c+'a'-'A');if(c!=expected[i])return false;}
    return true;
}
}
bool parseHttpHeaders(const char* data,size_t length,HttpHeaders& result) {
    result={};
    if(!data || length<18 || length>2048 || std::memcmp(data+length-4,"\r\n\r\n",4)!=0)return false;
    for(size_t i=0;i<length;++i)if(data[i]==0)return false;
    if(std::memcmp(data,"HTTP/1.0 ",9)!=0 && std::memcmp(data,"HTTP/1.1 ",9)!=0)return false;
    if(data[9]<'1' || data[9]>'5' || data[10]<'0' || data[10]>'9' || data[11]<'0' || data[11]>'9' || data[12]!=' ')return false;
    size_t position=13;
    while(position+1<length && !(data[position]=='\r' && data[position+1]=='\n')) {
        if(static_cast<unsigned char>(data[position])<32)return false;++position;
    }
    if(position+1>=length)return false;
    result.status=(data[9]-'0')*100+(data[10]-'0')*10+data[11]-'0';position+=2;
    bool foundLength=false;
    while(position+2<length) {
        size_t end=position;while(end+1<length && !(data[end]=='\r' && data[end+1]=='\n'))++end;
        if(end+1>=length || data[position]==' ' || data[position]=='\t')return false;
        size_t colon=position;
        while(colon<end && data[colon]!=':') {
            const char c=data[colon];if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-'))return false;++colon;
        }
        if(colon==position || colon==end)return false;
        if(equal(data+position,colon-position,"transfer-encoding"))return false;
        if(equal(data+position,colon-position,"content-length")) {
            if(foundLength)return false;foundLength=true;
            size_t start=colon+1;while(start<end && (data[start]==' ' || data[start]=='\t'))++start;
            size_t size=0;bool digit=false;
            while(start<end && data[start]>='0' && data[start]<='9') {digit=true;size=size*10+size_t(data[start++]-'0');if(size>ResponseBytes)return false;}
            while(start<end && (data[start]==' ' || data[start]=='\t'))++start;
            if(!digit || start!=end)return false;result.bodyLength=size;
        }
        position=end+2;
    }
    // Error responses which never confirm a sample do not need a body.
    return position==length-2 && (foundLength || result.status==401 || result.status==403 || result.status>=500 || result.status==404 || result.status==429);
}
}
