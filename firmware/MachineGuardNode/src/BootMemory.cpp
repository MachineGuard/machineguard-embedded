#include "BootMemory.h"
namespace mg {
namespace {
constexpr uint32_t Magic=0x4d475231;
uint32_t checksum(uint64_t boot,uint16_t pending) {
    uint32_t crc=0xffffffffU;
    for(unsigned i=0;i<10;++i) {
        uint8_t byte=i<8?uint8_t(boot>>(i*8)):uint8_t(pending>>((i-8)*8));crc^=byte;
        for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320U:0);
    }
    return ~crc;
}
}
RetainedMemory checkpoint(uint64_t boot,uint16_t pending) {return {boot,Magic,checksum(boot,pending),pending};}
bool previousPending(const RetainedMemory& r,bool softwareReset,uint64_t previousBoot,uint32_t& discarded) {
    discarded=0;
    if(!softwareReset || !previousBoot || r.magic!=Magic || r.bootCounter!=previousBoot || r.pending>MaxSamples
       || r.checksum!=checksum(r.bootCounter,r.pending))return false;
    discarded=r.pending;return true;
}
}
