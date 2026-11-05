#pragma once
#include <cstddef>
#include <cstdint>

namespace mg {
constexpr size_t MaxSamples=60, MaxRejected=8, PayloadBytes=512, ResponseBytes=1536;
constexpr int64_t MinimumEpochMs=1767225600000LL;
struct Sample {
    char sampleId[86]{};
    char capturedAt[25]{};
    int64_t epochMs=0;
    int16_t temperatureTenths=0, humidityTenths=0;
    bool hasCapturedAt=false;
};
struct CaptureTime { bool valid=false;int64_t epochMs=0; };
enum class Outcome : uint8_t { Retry, AuthRetry, InvalidReceipt, LocalPending, Confirmed,
    Discarded, Quarantined, EdgeOverflow, InvalidSample, Conflict };
struct Rejected { Sample sample{};Outcome reason=Outcome::Retry; };
struct Settings {
    char deviceCode[65]="ESP32-DEMO-01";
    uint32_t sampleMs=10000, sensorMinimumMs=2100, debounceMs=40, holdMs=5000,
             pairingMs=60000, retryMs=1000, retryMaximumMs=60000;
    size_t capacity=60;
    int16_t localTemperatureMinimum=20, localTemperatureMaximum=80,
            localHumidityMinimum=300, localHumidityMaximum=750;
    bool buzzerEnabled=false;
};
struct Stats {
    uint64_t acquired=0,sensorFailures=0,ramOverflow=0,localPending=0,confirmed=0,
        discarded=0,quarantined=0,edgeOverflow=0,rejected=0,rejectionEvidenceEvicted=0,
        retries=0,authFailures=0,invalidReceipts=0,clockRegressions=0,staleCompletions=0;
    uint32_t resetPendingDiscarded=0;
    bool resetLossKnown=false,sequenceExhausted=false;
};
struct Lights { bool green=false,amber=false,red=false,blue=false,buzzer=false; };
bool elapsed(uint32_t now,uint32_t since,uint32_t interval);
bool makeBootId(uint64_t mac,uint64_t bootCounter,const uint8_t entropy[16],char (&output)[65]);
bool nextBootCounter(uint64_t previous,uint64_t& next);
bool validSettings(const Settings& settings);
bool formatTimestamp(int64_t epochMs,char (&output)[25]);
bool encodePayload(const char* deviceCode,const Sample& sample,char* output,size_t capacity);

class Node {
public:
    Node(const Settings& settings,const char* bootId,uint32_t now,uint64_t initialSequence=0,
         bool resetLossKnown=false,uint32_t resetPending=0);
    void tick(uint32_t now,bool buttonPressed,bool networkConnected);
    bool beginRead(uint32_t now);
    bool finishRead(uint32_t now,float temperature,float humidity,CaptureTime time);
    bool beginSend(uint32_t now,Sample& copy,uint32_t& ticket);
    bool complete(uint32_t now,uint32_t ticket,Outcome outcome);
    void requestPairing(uint32_t now) {pairing_=true;pairStarted_=now;}
    size_t pending() const { return count_; }
    const Sample* oldest() const { return count_?&samples_[0]:nullptr; }
    const Rejected* lastRejected() const { return rejectionCount_?&rejected_[(rejectionCount_-1)%MaxRejected]:nullptr; }
    const Stats& stats() const { return stats_; }
    Lights lights(uint32_t now) const;
    bool pairing() const { return pairing_; }
    bool valid() const { return valid_; }
    bool busy() const { return inFlight_; }
private:
    void remove(size_t index);
    void retainRejection(const Sample&,Outcome);
    Settings settings_{};char bootId_[65]{};
    Sample samples_[MaxSamples]{};Rejected rejected_[MaxRejected]{};
    size_t count_=0;uint64_t rejectionCount_=0,nextSequence_=0;
    Stats stats_{};
    uint32_t lastSensor_=0,lastRetry_=0,backoff_=0,ticket_=0,rawChanged_=0,pressedAt_=0,pairStarted_=0;
    int64_t lastEpoch_=0;
    bool valid_=false,reading_=false,manual_=false,inFlight_=false,retrying_=false,
         rawButton_=false,button_=false,longSent_=false,pairing_=false,network_=false,
         sensorOk_=false,alarm_=false,muted_=false;
};
}
