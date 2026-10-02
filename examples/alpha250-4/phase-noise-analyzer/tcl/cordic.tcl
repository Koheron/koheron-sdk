namespace eval cordic {

proc pins {cmd} {
    $cmd -dir I -type clk      aclk
    $cmd -dir I -from 0  -to 0 aresetn
    $cmd -dir I -from 31 -to 0 s_axis_data_a
    $cmd -dir I -from [expr (1 + 2 * ([get_parameter dds_output_width] - 1) / 16) * 16 - 1] -to 0 s_axis_data_b
    $cmd -dir I -from 0  -to 0 s_axis_tvalid
    $cmd -dir O -from [expr [get_parameter phase_accumulator_width] - 1] -to 0 m_axis_tdata
    $cmd -dir O -from 0  -to 0 m_axis_tvalid
    $cmd -dir I -from 0  -to 0 acc_on
    $cmd -dir I -from 0  -to 0 rst_phase
    $cmd -dir O -from [get_parameter cordic_width] -to 0 freq
    $cmd -dir O -from [expr [get_parameter phase_accumulator_width] - 1] -to 0 phase
    $cmd -dir O -from 31 -to 0 demod
}

proc create {module_name rounding_seed} {

    set bd [current_bd_instance .]
    current_bd_instance [create_bd_cell -type hier $module_name]

    pins create_bd_pin

    # Preserve every product bit. CMPY Random_Rounding only randomizes ties;
    # full-product stochastic rounding below is unbiased for every fraction.

    # XAPP052 taps 64,63,61,60; XOR form excludes the zero state.
    # Legacy LFSR defaults remain unchanged for other instruments.
    cell pavel-demin:user:axis_lfsr:1.0 lfsr {
        SEED $rounding_seed
        FEEDBACK_MASK 0xd800000000000000
        FEEDBACK_XNOR 0
    } {
        aclk aclk
        aresetn aresetn
    }

    cell xilinx.com:ip:cmpy:6.0 complex_mult {
        APortWidth 16
        BPortWidth [get_parameter dds_output_width]
        OutputWidth [expr 17 + [get_parameter dds_output_width]]
        OptimizeGoal Performance
    } {
        aclk aclk
        s_axis_a_tdata s_axis_data_a
        s_axis_a_tvalid s_axis_tvalid
        s_axis_b_tdata s_axis_data_b
        s_axis_b_tvalid s_axis_tvalid
    }

    # CMPY pads each signed component to a byte boundary (33 -> 40 bits).
    set product_width [expr 17 + [get_parameter dds_output_width]]
    set product_stride [expr 8 * (($product_width + 7) / 8)]
    set mixer_salts {0x5bd1e9956c8e9cf5 0x27d4eb2f165667c5}
    for {set i 0} {$i < 2} {incr i} {
        # Separate I/Q generators avoid delayed copies of one random sequence.
        cell pavel-demin:user:axis_lfsr:1.0 mixer_lfsr$i {
            SEED [expr {$rounding_seed ^ [lindex $mixer_salts $i]}]
            FEEDBACK_MASK 0xd800000000000000
            FEEDBACK_XNOR 0
        } {
            aclk aclk
            aresetn aresetn
        }
        cell koheron:user:phase_round:1.0 mixer_round$i {
            INPUT_WIDTH $product_width
            SHIFT [expr $product_width - 16]
            OUTPUT_WIDTH 16
        } {
            clk aclk
            aresetn aresetn
            phase [get_slice_pin complex_mult/m_axis_dout_tdata [expr $product_stride * $i + $product_width - 1] [expr $product_stride * $i]]
            random_round [get_slice_pin mixer_lfsr$i/m_axis_tdata [expr $product_width - 17] 0]
        }
    }

    # Suppress the mixing image before nonlinear phase extraction.

    for {set i 0} {$i < 2} {incr i} {
        cell koheron:user:phase_prefilter:1.0 prefilter$i {
            OUTPUT_WIDTH [get_parameter cordic_width]
        } {
            clk aclk
            aresetn aresetn
            din mixer_round$i/rounded_phase
            random_round [get_slice_pin lfsr/m_axis_tdata [expr 31 + 16 * $i] [expr 16 + 16 * $i]]
        }
    }

    # Cordic

    cell xilinx.com:ip:cordic:6.0 cordic {
        Functional_Selection Translate
        Pipelining_Mode Maximum
        Phase_Format Scaled_Radians
        Input_Width [get_parameter cordic_width]
        Output_Width [get_parameter cordic_width]
        Round_Mode Round_Pos_Neg_Inf
    } {
        aclk aclk
        s_axis_cartesian_tvalid [get_constant_pin 1 1]
        s_axis_cartesian_tdata [get_concat_pin [list prefilter0/dout prefilter1/dout]]
        m_axis_dout_tvalid m_axis_tvalid
    }

    # Keep the existing signed 16-bit I/Q telemetry and power-readout scale.
    set iq_shift [expr [get_parameter cordic_width] - 16]
    cell xilinx.com:ip:xlconcat:2.1 demod_data {
        NUM_PORTS 2
        IN0_WIDTH 16
        IN1_WIDTH 16
    } {
        In0 [get_slice_pin prefilter0/dout [expr [get_parameter cordic_width] - 1] $iq_shift]
        In1 [get_slice_pin prefilter1/dout [expr [get_parameter cordic_width] - 1] $iq_shift]
        dout demod
    }

    # Phase unwrapping

    cell koheron:user:phase_unwrapper:1.0 phase_unwrapper {
        DIN_WIDTH [get_parameter cordic_width]
        DOUT_WIDTH [get_parameter phase_accumulator_width]
    } {
        clk aclk
        acc_on acc_on
        rst rst_phase
        phase_in [get_slice_pin cordic/m_axis_dout_tdata [expr 2 * [get_parameter cordic_width] - 1] [get_parameter cordic_width]]
        phase_out m_axis_tdata
        freq_out freq
        phase_out phase
    }

  current_bd_instance $bd
}

} ;# end spectrum namespace
