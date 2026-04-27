/// AxisStreamPacketMux driver
///
/// (c) Koheron

#ifndef __AXIS_STREAM_PACKET_MUX_HPP__
#define __AXIS_STREAM_PACKET_MUX_HPP__

#include "server/hardware/memory_manager.hpp"

#include <algorithm>
#include <cstdint>
#include <tuple>

class AxisStreamPacketMux {
  public:
    AxisStreamPacketMux() {
        write_ctrl(sel, length_beats, false);
    }

    // Select stream input: 0 -> s_axis_0, 1 -> s_axis_1
    void select_input(uint32_t input) {
        sel = input & 0x1;
        write_ctrl(sel, length_beats, false);
    }

    // Packet length in beats. In hardware, 0 is interpreted as 1 beat.
    void set_packet_length(uint32_t length) {
        length_beats = std::min(length, max_length);
        write_ctrl(sel, length_beats, false);
    }

    // Send a single packet with current selection and length.
    void trigger() {
        write_ctrl(sel, length_beats, true);
    }

    auto get_settings() {
        return std::tuple{sel, length_beats};
    }

  private:
    static constexpr uint32_t max_length = (1u << 14) - 1; // LENGTH[15:2]

    uint32_t sel = 0;
    uint32_t length_beats = 0;

    void write_ctrl(uint32_t input, uint32_t length, bool enable) {
        const uint32_t data = ((length & max_length) << 2) |
                              ((enable ? 1u : 0u) << 1) |
                              (input & 0x1);
        hw::get_memory<mem::mux>().write<0x0>(data);
    }
};

#endif // __AXIS_STREAM_PACKET_MUX_HPP__
