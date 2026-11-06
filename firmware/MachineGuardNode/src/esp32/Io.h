#pragma once
#include "../Node.h"
#include "../NetworkPolicy.h"
#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace mg {
struct SendJob { Sample sample{};uint32_t ticket=0;char deviceCode[65]{}; };
struct SendResult { uint32_t ticket=0;Outcome outcome=Outcome::Retry; };
class EdgeIo {
public:
    bool begin(const NetworkSettings& settings,const char* ntpServer);
    bool submit(const Sample& sample,uint32_t ticket,const char* deviceCode);
    bool receive(SendResult& result);
    bool connected() const {return wifi_.load() && edge_.load();}
    CaptureTime captureTime() const;
    uint32_t stackFreeMinimumBytes() const {return task_?uint32_t(uxTaskGetStackHighWaterMark(task_)):0;}
private:
    static void entry(void* self);
    void run();
    Outcome exchange(const SendJob& job);
    NetworkSettings settings_{};const char* ntpServer_="";
    QueueHandle_t jobs_=nullptr,results_=nullptr;TaskHandle_t task_=nullptr;
    std::atomic<bool> wifi_{false},edge_{false};
};
}
