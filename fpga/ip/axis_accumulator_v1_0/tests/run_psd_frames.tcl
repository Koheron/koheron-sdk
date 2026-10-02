set root [file normalize [file join [file dirname [info script]] ../../../..]]
set out [file normalize [lindex $argv 0]]
if {$out eq $root} {error {Specify a simulation output directory}}
foreach flow {NonBlocking Blocking} {
    create_project -force psd_frames $out/$flow -part xc7z020clg400-2
    create_bd_design system
    source $root/fpga/lib/utilities.tcl
    create_bd_port -dir I -type clk clk
    set_property CONFIG.FREQ_HZ 250000000 [get_bd_ports clk]
    create_bd_port -dir I -from 63 -to 0 data
    foreach name {valid last} {create_bd_port -dir I $name}
    foreach name {ready out_valid out_last} {create_bd_port -dir O $name}
    create_bd_port -dir O -from 31 -to 0 power
    for {set i 0} {$i<2} {incr i} {
        cell xilinx.com:ip:floating_point:7.1 mult_$i [subst {
            Operation_Type Multiply Flow_Control $flow Maximum_Latency True
            Has_A_TLAST true RESULT_TLAST_Behv Pass_A_TLAST
        }] {}
        connect_port_pin clk mult_$i/aclk
        set bits [get_slice_pin [get_bd_ports data] [expr 31+32*$i] [expr 32*$i]]
        foreach port {s_axis_a_tdata s_axis_b_tdata} {connect_pins mult_$i/$port $bits}
        foreach port {s_axis_a_tvalid s_axis_b_tvalid} {connect_port_pin valid mult_$i/$port}
        connect_port_pin last mult_$i/s_axis_a_tlast
    }
    set properties [list Flow_Control $flow Add_Sub_Value Add C_Mult_Usage No_Usage \
        Maximum_Latency True Has_A_TLAST true RESULT_TLAST_Behv Pass_A_TLAST]
    if {$flow eq "Blocking"} {lappend properties Has_RESULT_TREADY false}
    cell xilinx.com:ip:floating_point:7.1 add_0 $properties {
        S_AXIS_A mult_0/M_AXIS_RESULT S_AXIS_B mult_1/M_AXIS_RESULT
    }
    connect_port_pin clk add_0/aclk
    connect_port_pin power add_0/m_axis_result_tdata
    connect_port_pin out_valid add_0/m_axis_result_tvalid
    connect_port_pin out_last add_0/m_axis_result_tlast
    if {$flow eq "Blocking"} {
        connect_port_pin ready mult_0/s_axis_a_tready
    } else {connect_port_pin ready [get_constant_pin 1 1]}
    validate_bd_design
    save_bd_design
    generate_target all [get_files system.bd]
    add_files [make_wrapper -files [get_files system.bd] -top]
    add_files -fileset sim_1 $root/fpga/ip/axis_accumulator_v1_0/tests/psd_frame_tb.sv
    set_property top psd_frame_tb [get_filesets sim_1]
    set_property xsim.simulate.runtime 0ns [get_filesets sim_1]
    set_property xsim.elaborate.debug_level all [get_filesets sim_1]
    launch_simulation
    run all
    set passed [get_value -radix bin [get_objects /*/passed]]
    close_sim
    if {$passed ne "1"} {error "PSD TLAST regression failed: $flow"}
    close_project
}
