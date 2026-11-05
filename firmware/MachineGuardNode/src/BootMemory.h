#pragma once
#include "Node.h"
namespace mg {
struct RetainedMemory { uint64_t bootCounter;uint32_t magic,checksum;uint16_t pending; };
RetainedMemory checkpoint(uint64_t bootCounter,uint16_t pending);
bool previousPending(const RetainedMemory& retained,bool softwareReset,uint64_t previousBoot,uint32_t& discarded);
}
