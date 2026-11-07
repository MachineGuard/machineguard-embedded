#include "NetworkPolicy.h"
#include <cstring>
namespace mg {
namespace {
bool bounded(const char* value,size_t minimum,size_t maximum) {
    if(!value)return false;
    for(size_t i=0;i<=maximum;++i)if(!value[i])return i>=minimum;
    return false;
}
bool privateIp(const char* text) {
    unsigned octets[4]{};size_t position=0;
    for(unsigned part=0;part<4;++part) {
        unsigned digits=0;
        while(text[position]>='0' && text[position]<='9') {
            octets[part]=octets[part]*10+unsigned(text[position++]-'0');if(++digits>3 || octets[part]>255)return false;
        }
        if(!digits || (part<3?text[position++]!='.':text[position]!=0))return false;
    }
    return octets[0]==10 || (octets[0]==172 && octets[1]>=16 && octets[1]<=31) || (octets[0]==192 && octets[1]==168);
}
bool credentialAndTiming(const NetworkSettings& s) {
    if(!bounded(s.credential,32,200) || std::strncmp(s.credential,"mgn_",4)!=0 || !s.port
       || s.connectMs<100 || s.connectMs>5000 || s.readMs<100 || s.readMs>5000 || s.exchangeMs<1000 || s.exchangeMs>15000)return false;
    for(size_t i=0;s.credential[i];++i) {
        const char c=s.credential[i];if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c=='-' || c=='.'))return false;
    }
    return true;
}
// Simulation only: Wokwi-GUEST is an open access point and host.wokwi.internal is virtual.
bool validWokwiGateway(const NetworkSettings& s) {
    return !s.tls && !s.dedicatedBankHttp && std::strcmp(s.ssid,"Wokwi-GUEST")==0 && s.password[0]==0
        && std::strcmp(s.host,"host.wokwi.internal")==0 && credentialAndTiming(s);
}
}
bool validNetwork(const NetworkSettings& s) {
    if(!s.enabled)return true;
    if(s.wokwiGateway)return validWokwiGateway(s);
    if(!bounded(s.host,1,127) || !bounded(s.ssid,1,32) || !bounded(s.password,8,63) || !credentialAndTiming(s))return false;
    bool previousDot=true;
    for(size_t i=0;s.host[i];++i) {
        const char c=s.host[i];
        if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-' || c=='.'))return false;
        if(c=='.' && previousDot)return false;previousDot=c=='.';
    }
    if(previousDot)return false;
    if(s.tls)return bounded(s.caCertificate,54,4096) && std::strstr(s.caCertificate,"-----BEGIN CERTIFICATE-----")
        && std::strstr(s.caCertificate,"-----END CERTIFICATE-----");
    return s.dedicatedBankHttp && privateIp(s.host);
}
}
