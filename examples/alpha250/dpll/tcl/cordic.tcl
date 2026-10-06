namespace eval cordic {
variable extractor_source [file normalize [file join [file dirname [info script]] ../phase_extractor.v]]

proc pins {cmd {phase_width 24}} {
    $cmd -dir I -type clk      aclk
    $cmd -dir I -from 0  -to 0 aresetn
    $cmd -dir I -from 31 -to 0 s_axis_data_a
    $cmd -dir I -from 31 -to 0 s_axis_data_b
    $cmd -dir I -from 0  -to 0 s_axis_tvalid
    $cmd -dir O -from 31 -to 0 m_axis_tdata
    $cmd -dir O -from 0  -to 0 m_axis_tvalid
    $cmd -dir I -from 0  -to 0 acc_on
    $cmd -dir O -from 16 -to 0 freq
    $cmd -dir O -from 31 -to 0 phase
    $cmd -dir O -from [expr {$phase_width}] -to 0 freq_feedback
    $cmd -dir O -from [expr {$phase_width+15}] -to 0 phase_feedback
}

proc create {module_name {phase_implementation fast} {cartesian_width 24} {phase_width 24}} {

    if {$phase_implementation ni {vendor fast}} {error "Unknown phase extractor: $phase_implementation"}
    if {$phase_width ni {16 24}} {error "Unsupported phase width: $phase_width"}
    if {$cartesian_width ni {16 24}} {error "Unsupported Cartesian width: $cartesian_width"}
    if {$phase_implementation eq "fast"} {
        variable extractor_source
        add_files -norecurse $extractor_source
        add_files -norecurse [file join [file dirname $extractor_source] phase_residual.v]
    }

    set bd [current_bd_instance .]
    current_bd_instance [create_bd_cell -type hier $module_name]

    pins create_bd_pin $phase_width

    # Complex multiplier, rounded with a linear feedback shift register

    cell pavel-demin:user:axis_lfsr:1.0 lfsr {} {
        aclk aclk
        aresetn aresetn
    }

    cell xilinx.com:ip:cmpy:6.0 complex_mult {
        APortWidth 16
        BPortWidth 16
        OutputWidth $cartesian_width
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
            DATA_WIDTH $cartesian_width
            LOW_LATENCY 1
        } {
            clk aclk
            din [get_slice_pin complex_mult/m_axis_dout_tdata [expr $cartesian_width - 1 + $cartesian_width * $i] [expr $cartesian_width * $i]]
        }
    }

    # Phase extraction. Retain the vendor core as a regression reference.
    if {$phase_implementation eq "vendor"} {
      cell xilinx.com:ip:cordic:6.0 cordic {
        Functional_Selection Translate
        Pipelining_Mode Optimal
        Phase_Format Scaled_Radians
        Input_Width $cartesian_width
        Output_Width $phase_width
        Round_Mode Round_Pos_Neg_Inf
    } {
        aclk aclk
        s_axis_cartesian_tvalid [get_constant_pin 1 1]
        s_axis_cartesian_tdata [get_concat_pin [list boxcar0/dout boxcar1/dout]]
        m_axis_dout_tvalid m_axis_tvalid
      }
      set phase_pin [get_slice_pin cordic/m_axis_dout_tdata [expr {2*$phase_width-1}] $phase_width]
    } else {
      create_bd_cell -type module -reference phase_extractor phase_extractor
      set_cell_props phase_extractor [list INPUT_WIDTH $cartesian_width PHASE_WIDTH $phase_width ITERATIONS $phase_width ROTATIONS_PER_CLOCK 2 PAIR_START 8 FUSE_ROUND 1 COMPACT_PREP 0 RESIDUAL_CORRECTION [expr {$phase_width == 24 && $cartesian_width == 24}]]
      connect_cell phase_extractor {
          clk aclk
          resetn aresetn
          valid_in [get_constant_pin 1 1]
          i_in boxcar0/dout
          q_in boxcar1/dout
          valid_out m_axis_tvalid
      }
      set phase_pin phase_extractor/phase_out
    }

    # Phase unwrapping

    cell koheron:user:phase_unwrapper:1.0 phase_unwrapper {
        DIN_WIDTH $phase_width
        DOUT_WIDTH [expr {$phase_width+16}]
    } {
        clk aclk
        acc_on acc_on
        phase_in $phase_pin
        phase_out phase_feedback
        freq_out freq_feedback
    }

    # Preserve existing monitor and direct DAC phase units. Controllers use
    # the full-precision outputs above, not these compatibility slices.
    set fractional_bits [expr {$phase_width-16}]
    set legacy_phase [get_slice_pin phase_unwrapper/phase_out [expr {$phase_width+15}] $fractional_bits]
    set legacy_freq [get_slice_pin phase_unwrapper/freq_out $phase_width $fractional_bits]
    connect_pins $legacy_phase phase
    connect_pins $legacy_phase m_axis_tdata
    connect_pins $legacy_freq freq

  current_bd_instance $bd
}

} ;# end spectrum namespace
