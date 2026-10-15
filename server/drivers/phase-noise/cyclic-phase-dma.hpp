#pragma once
#include "phase-ring.hpp"

struct Zynq7000PhaseDmaPort {
    static void initialize() {
        auto& hp = hw::get_memory<mem::axi_hp0>();
        hp.set_bit<0x0, 0>();
        hp.set_bit<0x14, 0>();
    }
};
using CyclicPhaseDma = BasicCyclicPhaseDma<1, Zynq7000PhaseDmaPort>;
using PairedPhaseDma = BasicCyclicPhaseDma<2, Zynq7000PhaseDmaPort>;
