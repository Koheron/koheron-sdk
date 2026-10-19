if {$argc != 1} {error "Expected an output directory"}
set out [file normalize [lindex $argv 0]]
set repo [file normalize [file join [file dirname [info script]] ../../../..]]
create_project -force multirate_frontend $out -part xczu3eg-sbva484-1-e
create_bd_design frontend_test
source $repo/fpga/lib/utilities.tcl
source $repo/examples/alpha250/dpll/tcl/multirate_frontend.tcl
foreach clock {clk reference_clk} {
    create_bd_port -dir I -type clk $clock
}
set_property CONFIG.FREQ_HZ 480000000 [get_bd_ports clk]
set_property CONFIG.FREQ_HZ 240000000 [get_bd_ports reference_clk]
create_bd_port -dir I -type rst resetn
set_property CONFIG.POLARITY ACTIVE_LOW [get_bd_ports resetn]
create_bd_port -dir I valid_in
foreach port {data_a data_b} {create_bd_port -dir I -from 31 -to 0 $port}
create_bd_port -dir I -from 7 -to 0 random_round
multirate_frontend::create frontend
foreach port {clk resetn valid_in data_a data_b random_round} {
    connect_bd_net [get_bd_ports $port] [get_bd_pins frontend/$port]
}
cell xilinx.com:ip:cmpy:6.0 reference_mixer {
    APortWidth 16 BPortWidth 16 OutputWidth 24
    OptimizeGoal Performance RoundMode Random_Rounding
    LatencyConfig Manual MinimumLatency 4 ARESETN true
} {}
foreach {pin port} {
    aclk reference_clk aresetn resetn
    s_axis_a_tdata data_a s_axis_b_tdata data_b s_axis_ctrl_tdata random_round
    s_axis_a_tvalid valid_in s_axis_b_tvalid valid_in s_axis_ctrl_tvalid valid_in
} {
    connect_bd_net [get_bd_ports $port] [get_bd_pins reference_mixer/$pin]
}
foreach {port pin width} {
    i_filtered frontend/i_filtered 16 q_filtered frontend/q_filtered 16
    valid_out frontend/valid_out 1 reference_data reference_mixer/m_axis_dout_tdata 48
    reference_valid reference_mixer/m_axis_dout_tvalid 1
} {
    create_bd_port -dir O -from [expr {$width-1}] -to 0 $port
    connect_bd_net [get_bd_ports $port] [get_bd_pins $pin]
}
validate_bd_design
save_bd_design
set bd [get_files */frontend_test.bd]
generate_target all $bd
add_files [make_wrapper -files $bd -top]
foreach name {table_gain.v table_corrector.v fast_p_detector.v p_path_switch.v manual_p_corrector.v} {
    add_files $repo/examples/alpha250/dpll/$name
}
add_files $repo/fpga/cores/latched_mux_v1_0/latched_mux.v
set_property verilog_define XILINX_ULTRASCALE [current_fileset]
add_files -fileset sim_1 $repo/examples/alpha250/dpll/tests/test_multirate_frontend_tb.sv
set_property verilog_define XILINX_ULTRASCALE [get_filesets sim_1]
set_property top test_multirate_frontend_tb [get_filesets sim_1]
set_property xsim.simulate.runtime all [get_filesets sim_1]
launch_simulation
close_sim
source $repo/examples/alpha250/dpll/tests/check_simulation.tcl
check_simulation $out/multirate_frontend.sim/sim_1/behav/xsim/simulate.log {Multirate frontend checks passed:}
close_project
