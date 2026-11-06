#pragma once
#include "src/Node.h"
#include "src/NetworkPolicy.h"
#include <cstring>
#ifndef MG_SIMULATION_PROFILE
#define MG_SIMULATION_PROFILE 0
#endif
#if MG_SIMULATION_PROFILE
#if __has_include("secrets.h")
#error "Offline simulation profile must not be compiled alongside secrets.h"
#endif
#include "secrets.example.h"
#else
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif
#endif

namespace config {
constexpr int DhtPin=4,GreenPin=25,AmberPin=26,RedPin=27,BluePin=33,ButtonPin=14,BuzzerPin=32;
constexpr bool SimulationProfile=MG_SIMULATION_PROFILE==1;
constexpr bool EnableBuzzer=SimulationProfile;
constexpr uint32_t SamplingMs=10000,SensorMinimumMs=2100;
constexpr size_t QueueCapacity=60;
inline mg::Settings nodeSettings() {
    mg::Settings s;
    static_assert(sizeof(MG_DEVICE_CODE)<=sizeof s.deviceCode,"deviceCode exceeds the supported 64 characters");
    std::strcpy(s.deviceCode,MG_DEVICE_CODE);s.sampleMs=SamplingMs;s.sensorMinimumMs=SensorMinimumMs;s.capacity=QueueCapacity;
    s.buzzerEnabled=EnableBuzzer;
    // Demo limits are local. They do not configure Core or resolve its incidents.
    s.localTemperatureMinimum=20;s.localTemperatureMaximum=80;
    s.localHumidityMinimum=300;s.localHumidityMaximum=750;return s;
}
inline mg::NetworkSettings networkSettings() {
    mg::NetworkSettings s;s.enabled=MG_NETWORK_ENABLED;s.tls=MG_EDGE_TLS;s.dedicatedBankHttp=MG_DEDICATED_BANK_HTTP;
    s.host=MG_EDGE_HOST;s.port=MG_EDGE_PORT;s.ssid=MG_WIFI_SSID;s.password=MG_WIFI_PASSWORD;
    s.credential=MG_NODE_CREDENTIAL;s.caCertificate=MG_EDGE_CA_PEM;return s;
}
static_assert(!SimulationProfile || !MG_NETWORK_ENABLED,"Simulation profile must remain offline");
}
