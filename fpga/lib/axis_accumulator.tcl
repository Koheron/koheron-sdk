# Adapter for real-time framed PSD streams and BRAM recorders. The packaged
# IP itself exposes normal AXI4-Stream interfaces, including TREADY and TLAST.
namespace eval axis_accumulator {
proc create {module_name frame_length n_frames} {
    # These upstream pipelines cannot stall. With an always-ready recorder,
    # these dimensions leave enough time to drain a bank before it is reused.
    if {$frame_length < 16 || $n_frames < 3} {
        error {Real-time PSD adapter requires at least 16 bins and 3 frames; use the native AXIS IP for shorter frames or backpressure.}
    }
    set bd [current_bd_instance .]
    current_bd_instance [create_bd_cell -type hier $module_name]
    create_bd_pin -dir I -type clk clk
    create_bd_pin -dir I -type rst resetn
    create_bd_pin -dir I -from 31 -to 0 s_axis_tdata
    create_bd_pin -dir I s_axis_tvalid
    create_bd_pin -dir I s_axis_tlast
    create_bd_pin -dir O -from 31 -to 0 m_axis_tdata
    create_bd_pin -dir O -from 31 -to 0 addr_out
    create_bd_pin -dir O -from 31 -to 0 cycle_index
    create_bd_pin -dir O -from 3 -to 0 wen
    cell koheron:user:axis_accumulator:1.0 accum [subst {
        FRAME_LENGTH $frame_length N_FRAMES $n_frames CHECK_TLAST 1 SYNC_ON_RESET 1
    }] {
        aclk clk
        aresetn resetn
        s_axis_tdata s_axis_tdata
        s_axis_tvalid s_axis_tvalid
        s_axis_tlast s_axis_tlast
        m_axis_tready [get_constant_pin 1 1]
        m_axis_tdata m_axis_tdata
        cycle_index cycle_index
    }
    connect_pins addr_out [get_concat_pin [list [get_constant_pin 0 2] [get_slice_pin accum/m_axis_tuser 29 0]]]
    connect_pins wen [get_concat_pin [lrepeat 4 accum/m_axis_tvalid]]
    current_bd_instance $bd
}
}
