#include "../p_path_control.hpp"
#include <array>
#include <cassert>
#include <iostream>
#include <vector>

namespace {
constexpr uint32_t command=0, coeff=4, integrators=20, status=0, snapshot=8;
uint32_t pack(int16_t i, int16_t q) {
    return uint16_t(i) | (uint32_t(uint16_t(q)) << 16);
}
struct Board {
    std::array<uint32_t, 7> ctl{};
    std::array<uint32_t, 4> sts{};
    std::vector<std::pair<uint32_t, uint32_t>> writes;
    bool stall=false;
    bool stall_mode=false;
    unsigned ticks=0;
    void tick() {
        if (stall || ++ticks % 9) return;
        for (unsigned channel=0; channel<2; ++channel) {
            const uint32_t request=ctl[0];
            sts[channel]=(sts[channel]&~5U) | (stall_mode ? 0U : ((request>>channel)&1U)) |
                (((request>>(8+channel))&1U)<<2);
        }
    }
};
struct Control {
    Board& board;
    uint32_t read_reg(uint32_t offset) { return board.ctl.at(offset/4); }
    void write_reg(uint32_t offset, uint32_t value) {
        board.writes.emplace_back(offset,value); board.ctl.at(offset/4)=value;
    }
};
struct Status {
    Board& board;
    uint32_t read_reg(uint32_t offset) { board.tick(); return board.sts.at(offset/4); }
};
}

int main() {
    int checked=0;
    // Compare the calibrated projection against sin(residual) at reference
    // angles across the full circle, including axes, diagonals and the wrap.
    for (int degrees=-180; degrees<=180; degrees+=3) {
        const double angle=degrees*3.14159265358979323846/180;
        for (int amplitude : {64,128,4096,30000}) {
            const int16_t i=std::lround(amplitude*std::cos(angle));
            const int16_t q=std::lround(amplitude*std::sin(angle));
            int32_t cx=0,cy=0;
            if (double(i)*i + double(q)*q < 4096) {
                assert(!dpll_p::calibrate(pack(i,q),cx,cy));
                continue;
            }
            assert(dpll_p::calibrate(pack(i,q),cx,cy));
            const double actual_angle=std::atan2(q,i);
            const double actual_amplitude=std::hypot(i,q);
            for (int residual=-14; residual<=14; ++residual) {
                const double d=residual*3.14159265358979323846/180;
                const double x=actual_amplitude*std::cos(actual_angle+d);
                const double y=actual_amplitude*std::sin(actual_angle+d);
                const double estimated=(x*cx+y*cy)/262144;
                const double expected=8192/3.14159265358979323846*std::sin(d);
                assert(std::abs(estimated-expected)<0.09);
                ++checked;
            }
        }
    }
    int32_t cx=17,cy=19;
    assert(!dpll_p::calibrate(pack(0,0),cx,cy));
    assert(!dpll_p::calibrate(pack(63,0),cx,cy));
    assert(cx==17 && cy==19);
    assert(dpll_p::calibrate(pack(-32768,-32768),cx,cy));
    Board board;
    board.ctl[integrators/4]=board.ctl[integrators/4+1]=5;
    board.sts[snapshot/4]=pack(4096,0);
    board.sts[snapshot/4+1]=pack(0,-4096);
    Control control{board}; Status readback{board};
    dpll_p::Paths paths(control,readback,command,coeff,status,snapshot,integrators);
    assert(paths.select(2,0)==-1 && paths.select(0,2)==-1);
    assert(board.writes.empty());
    for (const uint32_t bits : {0U,1U,2U,3U,4U,6U}) {
        board.ctl[integrators/4]=bits;
        assert(paths.select(0,1)==-3);
    }
    board.ctl[integrators/4]=5;
    assert(board.writes.empty());
    assert(paths.select(1,1)==0);
    assert((board.sts[1]&1) && !(board.sts[0]&1));
    assert(board.ctl[coeff/4]==0 && board.ctl[coeff/4+1]==0);
    assert(int32_t(board.ctl[coeff/4+2])>0 && board.ctl[coeff/4+3]==0);
    const auto writes=board.writes.size();
    assert(paths.select(1,1)==0 && board.writes.size()==writes);
    assert(paths.select(0,1)==0 && (board.ctl[0]&3)==3);
    assert(paths.select(1,0)==0 && (board.ctl[0]&3)==1);
    // Restart uses hardware readback, including the held capture toggle.
    dpll_p::Paths restarted(control,readback,command,coeff,status,snapshot,integrators);
    assert(restarted.select(0,0)==0);
    board.sts[snapshot/4]=pack(0,0);
    assert(restarted.select(0,1)==-3 && !(board.ctl[0]&1));
    board.ctl[integrators/4]=0;
    assert(restarted.select(0,1)==-3);
    board.ctl[integrators/4]=5;
    board.sts[snapshot/4]=pack(4096,0);
    board.stall=true;
    assert(restarted.select(0,1)==-2 && !(board.ctl[0]&1));
    board.stall=false;
    assert(restarted.select(0,1)==0);
    assert(restarted.select(0,0)==0);
    board.stall_mode=true;
    assert(restarted.select(0,1)==-2 && !(board.ctl[0]&1));
    board.stall_mode=false;
    assert(restarted.select(0,1)==0);
    std::cout << "P path host checks passed: " << checked << " projections plus protocol cases\n";
}
