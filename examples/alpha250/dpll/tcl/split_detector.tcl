add_files -norecurse [file normalize [file join [file dirname [info script]] .. accurate_phase_consumers.v]]
namespace eval split_detector {
variable source_directory [file normalize [file join [file dirname [info script]] ..]]

proc pins {cmd} {
    $cmd -dir I -type clk      aclk
    $cmd -dir I -from 0  -to 0 aresetn
    $cmd -dir I -from 31 -to 0 s_axis_data_a
    $cmd -dir I -from 31 -to 0 s_axis_data_b
    $cmd -dir I -from 0  -to 0 s_axis_tvalid
    $cmd -dir O -from 31 -to 0 m_axis_tdata
    $cmd -dir O -from 0  -to 0 m_axis_tvalid
    $cmd -dir I -from 0  -to 0 acc_on
    $cmd -dir O -from 24 -to 0 freq
    $cmd -dir O -from 39 -to 0 phase
    $cmd -dir O -from 15 -to 0 i_filtered
    $cmd -dir O -from 15 -to 0 q_filtered
    $cmd -dir O -from 63 -to 0 monitor_phase
    $cmd -dir O -from 31 -to 0 demod
}

proc create {module_name {seed 0x9e3779b97f4a7c15} {phase_implementation fast}} {

    if {$phase_implementation ni {fast vendor}} {error "Unknown accurate phase extractor: $phase_implementation"}
    if {$phase_implementation eq "fast"} {
        variable source_directory
        foreach source {phase_extractor.v phase_residual.v} {
            add_files -norecurse [file join $source_directory $source]
        }
    }

    set bd [current_bd_instance .]
    current_bd_instance [create_bd_cell -type hier $module_name]

    pins create_bd_pin

    # Complex multiplier, rounded with a linear feedback shift register

    cell pavel-demin:user:axis_lfsr:1.0 lfsr [list SEED $seed FEEDBACK_MASK 0xd800000000000000 FEEDBACK_XNOR 0] {
        aclk aclk
        aresetn aresetn
    }

    cell xilinx.com:ip:cmpy:6.0 complex_mult {
        APortWidth 16
        BPortWidth 16
        OutputWidth 24
        OptimizeGoal Performance
        RoundMode Random_Rounding
    } {
        aclk aclk
        s_axis_a_tdata s_axis_data_a
        s_axis_a_tvalid s_axis_tvalid
        s_axis_b_tdata s_axis_data_b
        s_axis_b_tvalid s_axis_tvalid
        s_axis_ctrl_tdata lfsr/m_axis_tdata
        s_axis_ctrl_tvalid lfsr/m_axis_tvalid
    }

    # Filter the multiplier output with a boxcar filter

    for {set i 0} {$i < 2} {incr i} {
        cell koheron:user:boxcar_filter:1.0 boxcar$i {
            DATA_WIDTH 16
            LOW_LATENCY 1
        } {
            clk aclk
            din [get_slice_pin complex_mult/m_axis_dout_tdata [expr 23 + 24 * $i] [expr 8 + 24 * $i]]
        }
    }

    connect_pins boxcar0/dout i_filtered
    connect_pins boxcar1/dout q_filtered
    for {set i 0} {$i < 2} {incr i} {
        cell koheron:user:phase_prefilter:1.0 prefilter$i {WIDTH 24} {
            clk aclk aresetn aresetn
            din [get_slice_pin complex_mult/m_axis_dout_tdata [expr 23+24*$i] [expr 24*$i]]
            random_round [get_slice_pin lfsr/m_axis_tdata [expr 31+16*$i] [expr 16+16*$i]]
        }
    }
    if {$phase_implementation eq "vendor"} {
      cell xilinx.com:ip:cordic:6.0 cordic {
        Functional_Selection Translate Pipelining_Mode Maximum
        Phase_Format Scaled_Radians Input_Width 24 Output_Width 24
        Round_Mode Round_Pos_Neg_Inf
    } {
        aclk aclk s_axis_cartesian_tvalid [get_constant_pin 1 1]
        s_axis_cartesian_tdata [get_concat_pin [list prefilter0/dout prefilter1/dout] accurate_cartesian]
        m_axis_dout_tvalid m_axis_tvalid
      }
      set phase_pin [get_slice_pin cordic/m_axis_dout_tdata 47 24]
    } else {
      create_bd_cell -type module -reference phase_extractor phase_extractor
      set_cell_props phase_extractor {
          INPUT_WIDTH 24 PHASE_WIDTH 24 ITERATIONS 24
          ROTATIONS_PER_CLOCK 2 PAIR_START 8 FUSE_ROUND 1
          COMPACT_PREP 0 RESIDUAL_CORRECTION 1
      }
      connect_cell phase_extractor {
          clk aclk resetn aresetn valid_in [get_constant_pin 1 1]
          i_in prefilter0/dout q_in prefilter1/dout valid_out m_axis_tvalid
      }
      set phase_pin phase_extractor/phase_out
    }
    cell koheron:user:phase_unwrapper:1.0 phase_unwrapper {
        DIN_WIDTH 24 DOUT_WIDTH 64 PIPELINED_OVERFLOW 1 PIPELINED_HISTORY 1
    } {
        clk aclk acc_on [get_constant_pin 1 1] rst [get_not_pin aresetn]
        phase_in $phase_pin
        freq_out freq
    }
    set_cell_props phase_unwrapper [list FUSED_DIFFERENCE [expr {$phase_implementation eq "fast"}] CANONICAL_INPUT [expr {$phase_implementation eq "fast"}]]
    cell pavel-demin:user:axis_lfsr:1.0 phase_lfsr [list SEED [format 0x%016llx [expr {$seed ^ 0xa0761d6478bd642f}]] FEEDBACK_MASK 0xd800000000000000 FEEDBACK_XNOR 0] {aclk aclk aresetn aresetn}
    create_bd_cell -type module -reference accurate_phase_consumers consumers
    connect_cell consumers {
        clk aclk resetn aresetn acc_on acc_on
        frequency phase_unwrapper/freq_out phase phase_unwrapper/phase_out
        random_round [get_slice_pin phase_lfsr/m_axis_tdata 7 0]
        feedback_phase phase monitor_phase monitor_phase
    }
    connect_pins [get_slice_pin consumers/feedback_phase 39 8] m_axis_tdata
    connect_pins [get_concat_pin [list [get_slice_pin prefilter0/dout 23 8] [get_slice_pin prefilter1/dout 23 8]] demod_words] demod
    current_bd_instance $bd
}
}
