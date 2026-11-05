#include <Arduino.h>
#include <DHT.h>
#include <Preferences.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <esp_system.h>
#include <new>
#include "Config.h"
#include "src/BootMemory.h"
#include "src/esp32/Io.h"

namespace {
DHT dht(config::DhtPin,DHT22);
mg::EdgeIo io;
alignas(mg::Node) uint8_t nodeMemory[sizeof(mg::Node)];
mg::Node* node=nullptr;
RTC_NOINIT_ATTR mg::RetainedMemory retained;
uint64_t bootCounter=0;
bool ioEnabled=false,buzzerAttached=false,previousBuzzer=false;
uint32_t lastSummary=0;

bool identity(char (&id)[65],bool& resetKnown,uint32_t& resetPending) {
    Preferences store;
    if(!store.begin("mg-identity",false))return false;
    const uint64_t previous=store.isKey("boot")?store.getULong64("boot",UINT64_MAX):0;
    resetKnown=mg::previousPending(retained,esp_reset_reason()==ESP_RST_SW,previous,resetPending);
    retained.magic=0;
    if(!mg::nextBootCounter(previous,bootCounter) || store.putULong64("boot",bootCounter)!=sizeof(uint64_t)
       || store.getULong64("boot",0)!=bootCounter) {store.end();return false;}
    store.end();
    uint8_t macBytes[6]{},entropy[16]{};
    if(esp_read_mac(macBytes,ESP_MAC_WIFI_STA)!=ESP_OK)return false;
    uint64_t mac=0;for(uint8_t byte:macBytes)mac=(mac<<8)|byte;
    // The persisted counter also separates boots when RF entropy is not available offline.
    esp_fill_random(entropy,sizeof entropy);
    return mg::makeBootId(mac,bootCounter,entropy,id);
}
void rememberPending() {
    const auto next=mg::checkpoint(bootCounter,uint16_t(node->pending()));
    retained.bootCounter=next.bootCounter;retained.pending=next.pending;retained.checksum=next.checksum;
    retained.magic=next.magic; // Commit last. An interrupted update is reported as unknown.
}
}

void setup() {
    Serial.begin(115200);
    for(int pin:{config::GreenPin,config::AmberPin,config::RedPin,config::BluePin,config::BuzzerPin}) {pinMode(pin,OUTPUT);digitalWrite(pin,LOW);}
    pinMode(config::ButtonPin,INPUT_PULLUP);dht.begin();
    if(config::EnableBuzzer)buzzerAttached=ledcAttach(config::BuzzerPin,4000,8);
    char bootId[65]{};bool resetKnown=false;uint32_t resetPending=0;
    const bool ready=identity(bootId,resetKnown,resetPending);
    const auto settings=config::nodeSettings();
    node=new(nodeMemory) mg::Node(settings,ready?bootId:"",millis(),0,resetKnown,resetPending);
    if(!ready)Serial.println("IDENTITY_STORAGE_ERROR: acquisition disabled");
    if(resetKnown)Serial.printf("RESET_RAM_DISCARDED=%u EDGE_DELIVERY_UNKNOWN\n",unsigned(resetPending));
    else Serial.println("RESET_RAM_LOSS=UNKNOWN; payloads are not persisted");
    const auto network=config::networkSettings();
    const bool started=io.begin(network,MG_NTP_SERVER);
    ioEnabled=network.enabled && started && mg::validNetwork(network);
    if(!ioEnabled) {Serial.println("NETWORK_DISABLED_OR_UNPROVISIONED");node->requestPairing(millis());}
    if(node->valid())rememberPending();
}

void loop() {
    const uint32_t now=millis();
    retained.magic=0; // No exact reset count is claimed during an incomplete state update.
    node->tick(now,digitalRead(config::ButtonPin)==LOW,io.connected());
    if(!ioEnabled && !node->pairing())node->requestPairing(now);
    mg::SendResult result;
    if(io.receive(result))node->complete(now,result.ticket,result.outcome);
    if(node->beginRead(now)) {
        // One physical DHT transaction. Humidity comes from that same frame, not a second sample.
        const float temperature=dht.readTemperature(false,true);
        const float humidity=dht.readHumidity(false);
        node->finishRead(millis(),temperature,humidity,io.captureTime());
    }
    if(ioEnabled) {
        mg::Sample copy{};uint32_t ticket=0;
        if(node->beginSend(millis(),copy,ticket) && !io.submit(copy,ticket,config::nodeSettings().deviceCode))
            node->complete(millis(),ticket,mg::Outcome::Retry);
    }
    const auto lights=node->lights(millis());
    digitalWrite(config::GreenPin,lights.green);digitalWrite(config::AmberPin,lights.amber);
    digitalWrite(config::RedPin,lights.red);digitalWrite(config::BluePin,lights.blue);
    if(buzzerAttached && lights.buzzer!=previousBuzzer) {ledcWriteTone(config::BuzzerPin,lights.buzzer?4000:0);previousBuzzer=lights.buzzer;}
    if(node->valid())rememberPending();
    if(mg::elapsed(millis(),lastSummary,5000)) {
        lastSummary=millis();const auto& stats=node->stats();
        Serial.printf("RAM=%u LOCAL_PENDING=%llu CORE_CONFIRMED=%llu RAM_OVERFLOW=%llu TERMINAL=%llu\n",
            unsigned(node->pending()),static_cast<unsigned long long>(stats.localPending),static_cast<unsigned long long>(stats.confirmed),
            static_cast<unsigned long long>(stats.ramOverflow),static_cast<unsigned long long>(stats.discarded+stats.quarantined+stats.edgeOverflow+stats.rejected));
    }
    delay(1); // Yield only; all long network work belongs to the single IO task.
}
