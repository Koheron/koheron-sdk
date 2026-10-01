set root [file normalize [file join [file dirname [info script]] ..]]
create_project control_tests [file normalize tmp/dds-pm-control-sim] -part xc7z020clg400-2 -force
add_files [glob $root/*.v]
add_files -fileset sim_1 $root/tests/control_tb.sv
set_property top control_tb [get_filesets sim_1]
set_property xsim.simulate.runtime 0ns [get_filesets sim_1]
foreach width {32 33 48} {
    set_property verilog_define PHASE_TEST_WIDTH=$width [get_filesets sim_1]
    launch_simulation
    run all
    if {[get_value -radix unsigned /control_tb/passed] != 1} { error "Control test failed at $width bits" }
    close_sim
}
close_project
