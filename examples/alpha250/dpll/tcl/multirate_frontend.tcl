# Optional Fast frontend for a processing clock faster than the sample rate.
# Board integration supplies coherent ADC/DDS/rounding words and a valid epoch.
namespace eval multirate_frontend {
variable directory [file normalize [file join [file dirname [info script]] ..]]

proc create {name} {
    variable directory
    add_files -norecurse [list $directory/sampled_mixer_input.v $directory/sampled_boxcar.v]
    set previous [current_bd_instance .]
    current_bd_instance [create_bd_cell -type hier $name]
    create_bd_pin -dir I -type clk clk
    create_bd_pin -dir I -type rst resetn
    create_bd_pin -dir I valid_in
    foreach pin {data_a data_b} {create_bd_pin -dir I -from 31 -to 0 $pin}
    create_bd_pin -dir I -from 7 -to 0 random_round
    foreach pin {i_filtered q_filtered} {create_bd_pin -dir O -from 15 -to 0 $pin}
    create_bd_pin -dir O valid_out

    create_bd_cell -type module -reference sampled_mixer_input input_epoch
    connect_cell input_epoch {
        clk clk resetn resetn valid_in valid_in
        a_in data_a b_in data_b rounding_in random_round
    }
    cell xilinx.com:ip:cmpy:6.0 complex_mult {
        APortWidth 16 BPortWidth 16 OutputWidth 24
        OptimizeGoal Performance RoundMode Random_Rounding
        LatencyConfig Manual MinimumLatency 4 ARESETN true
    } {
        aclk clk aresetn resetn
        s_axis_a_tdata input_epoch/a_out s_axis_b_tdata input_epoch/b_out
        s_axis_ctrl_tdata input_epoch/rounding_out
        s_axis_a_tvalid input_epoch/valid_out
        s_axis_b_tvalid input_epoch/valid_out
        s_axis_ctrl_tvalid input_epoch/valid_out
    }
    foreach channel {0 1} output {i_filtered q_filtered} {
        create_bd_cell -type module -reference sampled_boxcar boxcar$channel
        connect_cell boxcar$channel {
            clk clk resetn resetn valid_in complex_mult/m_axis_dout_tvalid
            din [get_slice_pin complex_mult/m_axis_dout_tdata [expr {23+24*$channel}] [expr {8+24*$channel}]]
            dout $output
        }
    }
    connect_pins boxcar0/valid_out valid_out
    current_bd_instance $previous
}
}
