#pragma once
#include <cstdint>
namespace mg {
struct NetworkSettings {
    bool enabled=false,tls=true,dedicatedBankHttp=false;
    const char* host="";uint16_t port=8443;
    const char* ssid="";const char* password="";const char* credential="";const char* caCertificate="";
    uint32_t connectMs=1500,readMs=1500,exchangeMs=6000;
};
bool validNetwork(const NetworkSettings& settings);
}
