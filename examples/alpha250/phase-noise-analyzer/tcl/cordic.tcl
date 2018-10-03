namespace eval cordic {

proc pins {cmd} {
    $cmd -dir I -type clk      aclk
    $cmd -dir I -from 0  -to 0 aresetn
    $cmd -dir I -from 31 -to 0 s_axis_data_a
    $cmd -dir I -from 31 -to 0 s_axis_data_b
    $cmd -dir I -from 0  -to 0 s_axis_tvalid
    $cmd -dir O -from 31 -to 0 m_axis_tdata
    $cmd -dir O -from 0  -to 0 m_axis_tvalid
    $cmd -dir I -from 0  -to 0 acc_on
    $cmd -dir I -from 0  -to 0 rst_phase
    $cmd -dir O -from 16 -to 0 freq
    $cmd -dir O -from 31 -to 0 phase
}

proc create {module_name} {

    set bd [current_bd_instance .]
    current_bd_instance [create_bd_cell -type hier $module_name]

    pins create_bd_pin

    # Complex multiplier, rounded with a linear feedback shift register

    cell pavel-demin:user:axis_lfsr:1.0 lfsr {} {
        aclk aclk
        aresetn aresetn
    }

    cell xilinx.com:ip:cmpy:6.0 complex_mult {
        APortWidth 16
        BPortWidth 16
        OutputWidth 16
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
        } {
            clk aclk
            din [get_slice_pin complex_mult/m_axis_dout_tdata [expr 15 + 16 * $i] [expr 16 * $i]]
        }
    }

    # Cordic

    cell xilinx.com:ip:cordic:6.0 cordic {
        Functional_Selection Translate
        Pipelining_Mode Maximum
        Phase_Format Scaled_Radians
        Input_Width 16
        Output_Width 16
        Round_Mode Round_Pos_Neg_Inf
    } {
        aclk aclk
        s_axis_cartesian_tvalid [get_constant_pin 1 1]
        s_axis_cartesian_tdata [get_concat_pin [list boxcar0/dout boxcar1/dout]]
        m_axis_dout_tvalid m_axis_tvalid
    }

    # Phase unwrapping

    cell koheron:user:phase_unwrapper:1.0 phase_unwrapper {
        DIN_WIDTH 16
        DOUT_WIDTH 32
    } {
        clk aclk
        acc_on acc_on
        rst rst_phase
        phase_in [get_slice_pin cordic/m_axis_dout_tdata 31 16]
        phase_out m_axis_tdata
        freq_out freq
        phase_out phase
    }

  current_bd_instance $bd
}

} ;# end spectrum namespace