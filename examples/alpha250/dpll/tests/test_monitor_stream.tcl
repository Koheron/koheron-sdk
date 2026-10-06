# Simulate the actual IP configurations exported by the full DPLL design.
if {$argc != 1} {error "Expected the full DPLL project path"}
set root [file normalize [file join [file dirname [info script]] ../../../..]]
set project [file normalize [lindex $argv 0]]
set out [file join $root tmp/tests/alpha250-dpll/monitor-stream]
set inputs {}
foreach name {system_fir_0 system_phase_cic_clock_converter_0} {
    set input [file rootname $project].srcs/sources_1/bd/system/ip/$name/$name.xci
    if {![file exists $input]} {error "Missing production IP $name; run make xpr first"}
    lappend inputs [file normalize $input]
}
create_project -force monitor_stream_test $out -part xc7z020clg400-2
cd $out
set_property XPM_LIBRARIES {XPM_CDC} [current_project]
foreach input $inputs {import_ip -files $input}
generate_target simulation [get_ips]
add_files [list $root/fpga/cores/phase_stream_control_v1_0/phase_stream_control.v \
    $root/fpga/cores/phase_stream_cdc_v1_0/phase_stream_cdc.v \
    $root/fpga/cores/phase_fixed_decimator_v1_0/phase_fixed_decimator.v \
    $root/fpga/cores/phase_cic_decimator_v1_0/phase_cic_decimator.v]
add_files -fileset sim_1 $root/examples/alpha250/dpll/tests/test_monitor_stream_tb.sv
set_property top test_monitor_stream_tb [get_filesets sim_1]
set_property xsim.simulate.runtime all [get_filesets sim_1]
launch_simulation
close_sim
source $root/examples/alpha250/dpll/tests/check_simulation.tcl
check_simulation $out/monitor_stream_test.sim/sim_1/behav/xsim/simulate.log \
    {PASS: production CIC, asynchronous crossing and 143 MHz FIR throughput/reset recovery}
close_project
