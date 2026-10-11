#include "../sweep-plan.hpp"
#include <cassert>
#include <iostream>
#include <limits>
#include <string>
int main(int argc, char** argv) {
    assert(vna::valid_record(16u << 20, 12500000));
    assert(vna::valid_record(32u << 20, 31250000));
    assert(vna::valid_record(64u << 20, 62500000));
    assert(!vna::valid_record(16u << 20, 31250000));
    assert(!vna::valid_record((16u << 20) + 1024, 12500000));
    assert(!vna::valid_record(0, 12500000));
    assert(!vna::valid_record(128u << 20, 12500000));
    vna::SweepPlan p;
    assert(vna::make_plan(500,110e6,.25,-6,p));
    assert(p.samples == 62500000 && p.gain == 16423);
    for (const auto seed : p.seeds) assert(seed > 0 && seed < (1ULL << 63));
    assert(!vna::make_plan(500,125e6,.25,-6,p));
    assert(!vna::make_plan(500,110e6,.3,-6,p));
    assert(!vna::make_plan(500,110e6,.02,-6,p));
    assert(!vna::make_plan(500,110e6,.05,-6,p));
    assert(vna::make_plan(500e3,11e6,.02,-6,p));
    assert(!vna::make_plan(500,110e6,.25,-50,p));
    assert(!vna::make_plan(500,110e6,std::numeric_limits<double>::quiet_NaN(),-6,p));
    assert(vna::make_plan(500,55e6,.25,-6,p,125e6));
    assert(p.samples == 31250000 && p.gain == 16423);
    assert(!vna::make_plan(500,62.5e6,.25,-6,p,125e6));
    assert(!vna::make_plan(500,55e6,.05,-6,p,125e6));
    assert(vna::make_plan(500,55e6,.1,-6,p,125e6));
    assert(vna::make_plan(500,110e6,.25,-6,p));
    if (argc == 6) assert(vna::make_plan(std::stod(argv[1]), std::stod(argv[2]), std::stod(argv[3]), std::stod(argv[4]), p, std::stod(argv[5])));
    std::cout << '[' << p.samples << ',' << p.gain << ',' << uint32_t(p.coefficient) << ',' << uint32_t(p.coefficient >> 32);
    for (auto seed : p.seeds) std::cout << ',' << uint32_t(seed) << ',' << uint32_t(seed >> 32);
    std::cout << "]\n";
}
