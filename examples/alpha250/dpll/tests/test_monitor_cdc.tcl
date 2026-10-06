set root [file normalize [file join [file dirname [info script]] ../../../..]]
set out [file join $root tmp/tests/alpha250-dpll/monitor-cdc]
create_project -force monitor_cdc_test $out -part xc7z020clg400-2
cd $out
set_property XPM_LIBRARIES {XPM_CDC} [current_project]
add_files $root/fpga/cores/phase_stream_cdc_v1_0/phase_stream_cdc.v
add_files -fileset sim_1 $root/examples/alpha250/dpll/tests/test_monitor_cdc_tb.sv
set_property top test_monitor_cdc_tb [get_filesets sim_1]
set_property xsim.simulate.runtime all [get_filesets sim_1]
launch_simulation
close_sim
source $root/examples/alpha250/dpll/tests/check_simulation.tcl
check_simulation $out/monitor_cdc_test.sim/sim_1/behav/xsim/simulate.log \
    {PASS: monitor 250/143 MHz reset, metadata and coherent status}
close_project
