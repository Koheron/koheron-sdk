#pragma once
#include <array>
#include <cstdint>
#include <cstdlib>
namespace mem {
enum {control,status,adc,dac};
constexpr uint32_t adc_range=256*1024,dac_range=256*1024;
}
namespace reg { constexpr uint32_t trig=16,capture_complete=8; }
namespace hw {
struct Capture {
    uint32_t complete=1,trigger=0,requests=0,polls=0,reads=0;
    bool pending=false,stopped=false;
} inline capture;
template<unsigned ID> struct Memory {
    template<uint32_t Offset> uint32_t read() {
        if constexpr(ID==mem::control) return capture.trigger;
        if(capture.pending && !capture.stopped && ++capture.polls==4) {
            capture.pending=false;capture.complete^=1;
        }
        return capture.complete;
    }
    template<uint32_t Offset,unsigned Bit> void set_bit() {
        capture.trigger|=1u<<Bit;capture.pending=true;capture.polls=0;++capture.requests;
    }
    template<uint32_t Offset,unsigned Bit> void clear_bit() {capture.trigger&=~(1u<<Bit);}
    template<class T,size_t N> auto read_array() {
        if(capture.pending)std::abort();
        ++capture.reads;std::array<T,N> result;result.fill(capture.requests);return result;
    }
    template<class T,size_t N> void write_array(const std::array<T,N>&) {}
};
template<unsigned ID> Memory<ID>& get_memory(){static Memory<ID> memory;return memory;}
}
