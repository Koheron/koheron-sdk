if {$argc != 2} {error "Expected core directory and output directory"}
set cores [file normalize [lindex $argv 0]]
set out [file normalize [lindex $argv 1]]
set repo [file normalize [file join [file dirname [info script]] ../../../..]]
create_project -force p_frontend_test $out -part xc7z020clg400-2
set_property IP_REPO_PATHS $cores [current_project]
update_ip_catalog
create_bd_design p_frontend
source $repo/fpga/lib/utilities.tcl
source $repo/examples/alpha250/dpll/tcl/split_detector.tcl
create_bd_port -dir I -type clk aclk
set_property CONFIG.FREQ_HZ 250000000 [get_bd_ports aclk]
create_bd_port -dir I -type rst aresetn
set_property CONFIG.POLARITY ACTIVE_LOW [get_bd_ports aresetn]
foreach name {acc_on valid} {create_bd_port -dir I $name}
foreach name {data_a data_b} {create_bd_port -dir I -from 31 -to 0 $name}
split_detector::create detector
foreach {port pin} {aclk aclk aresetn aresetn acc_on acc_on valid s_axis_tvalid data_a s_axis_data_a data_b s_axis_data_b} {
    connect_bd_net [get_bd_ports $port] [get_bd_pins detector/$pin]
}
foreach {port pin width} {phase phase 40 freq freq 25 i_filtered i_filtered 16 q_filtered q_filtered 16} {
    create_bd_port -dir O -from [expr {$width-1}] -to 0 $port
    connect_bd_net [get_bd_ports $port] [get_bd_pins detector/$pin]
}
validate_bd_design
save_bd_design
set bd [get_files */p_frontend.bd]
generate_target all $bd
add_files [make_wrapper -files $bd -top]
foreach source {table_gain.v table_corrector.v fast_p_detector.v p_path_switch.v manual_p_corrector.v} {
    add_files $repo/examples/alpha250/dpll/$source
}
add_files -fileset sim_1 $repo/examples/alpha250/dpll/tests/test_p_frontend_tb.v
set_property top test_p_frontend_tb [get_filesets sim_1]
set_property xsim.simulate.runtime all [get_filesets sim_1]
launch_simulation
close_sim
close_project
