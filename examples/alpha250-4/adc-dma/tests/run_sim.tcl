set example [file normalize [file join [file dirname [info script]] ..]]
set output [file normalize tmp/alpha250-4-adc-dma-sim]
create_project quad_capture_tests $output -part xc7z020clg400-2 -force
add_files $example/quad_capture_v1_0/quad_capture.v
add_files -fileset sim_1 $example/tests/quad_capture_tb.sv
set_property top quad_capture_tb [get_filesets sim_1]
set_property xsim.simulate.runtime 0ns [get_filesets sim_1]
launch_simulation
run all
if {[get_value -radix unsigned /quad_capture_tb/passed] != 1} { error "quad_capture_tb failed" }
close_sim
close_project
