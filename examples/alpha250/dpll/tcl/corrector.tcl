namespace eval corrector {

proc pins {cmd} {
    $cmd -dir I -type clk clk
    # Inputs
    $cmd -dir I -from 31 -to 0 phase_in
    $cmd -dir I -from 16 -to 0 freq_in
    # Controller gains
    $cmd -dir I -from  2 -to 0 sclr
    $cmd -dir I -from 31 -to 0 p_gain
    $cmd -dir I -from 31 -to 0 pi_gain
    $cmd -dir I -from 31 -to 0 i2_gain
    $cmd -dir I -from 31 -to 0 i3_gain
    # Outputs
    $cmd -dir O -from 15 -to 0 fast_corr
    $cmd -dir O -from 15 -to 0 slow_corr
}

proc create {module_name} {

    set bd [current_bd_instance .]
    current_bd_instance [create_bd_cell -type hier $module_name]

    pins create_bd_pin

    cell xilinx.com:ip:mult_gen:12.0 proportional {
        PortAWidth 17
        PortBWidth 32
        OptGoal Speed
        PipeStages 3
        Use_Custom_Output_Width true
        OutputWidthHigh 31
        OutputWidthLow 0
    } {
        CLK clk
        A freq_in
        B p_gain
    }

    cell xilinx.com:ip:mult_gen:12.0 integral {
        PortAWidth 32
        PortBWidth 32
        OptGoal Speed
        PipeStages 3
        Use_Custom_Output_Width true
        OutputWidthHigh 47
        OutputWidthLow 16
    } {
        CLK clk
        A phase_in
        B pi_gain
    }

    cell xilinx.com:ip:c_addsub:12.0 first_adder {
        A_Width 32
        B_Width 32
        Add_Mode Add
        Out_Width 32
        Latency 1
        CE false
    } {
        CLK clk
        A proportional/P
        B integral/P
    }

    cell xilinx.com:ip:c_accum:12.0 first_accumulator {
        Input_Width 32
        Output_Width 48
        Bypass false
        SCLR true
    } {
        B first_adder/S
        CLK clk
        SCLR [get_not_pin [get_slice_pin sclr 0 0]]
    }

    cell xilinx.com:ip:mult_gen:12.0 double_integral {
        PortAWidth 48
        PortBWidth 32
        OptGoal Speed
        PipeStages 3
        Use_Custom_Output_Width true
        OutputWidthHigh 79
        OutputWidthLow 48
    } {
        CLK clk
        A first_accumulator/Q
        B i2_gain
    }

    cell xilinx.com:ip:c_addsub:12.0 second_adder {
        A_Width 32
        B_Width 32
        Add_Mode Add
        Out_Width 32
        Latency 1
        CE false
    } {
        CLK clk
        A first_adder/S
        B double_integral/P
    }

    cell xilinx.com:ip:c_accum:12.0 second_accumulator {
        Input_Width 32
        Output_Width 32
        Bypass false
        SCLR true
    } {
        B second_adder/S
        CLK clk
        SCLR [get_not_pin [get_slice_pin sclr 1 1]]
    }

    cell xilinx.com:ip:mult_gen:12.0 triple_integral {
        PortAWidth 32
        PortBWidth 32
        OptGoal Speed
        PipeStages 3
    } {
        CLK clk
        A second_accumulator/Q
        B i3_gain
    }

    cell xilinx.com:ip:c_accum:12.0 third_accumulator {
        Input_Width 64
        Output_Width 64
        Bypass false
        SCLR true
    } {
        B triple_integral/P
        CLK clk
        SCLR [get_not_pin [get_slice_pin sclr 2 2]]
    }

    connect_pin fast_corr [get_slice_pin second_accumulator/Q 31 16]

    # Convert from two's complement to binary representation for precision dac output
    connect_pin slow_corr [get_concat_pin [list [get_slice_pin third_accumulator/Q 62 48] [get_not_pin [get_slice_pin third_accumulator/Q 63 63]]]]

    current_bd_instance $bd
}

} ;# end corrector namespace
