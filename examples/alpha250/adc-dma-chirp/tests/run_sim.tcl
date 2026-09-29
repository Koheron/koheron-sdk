# vivado -mode batch -source examples/alpha250/adc-dma-chirp/tests/run_sim.tcl
set example [file normalize [file join [file dirname [info script]] ..]]
set output [file normalize tmp/adc-dma-chirp-sim]
create_project chirp_tests $output -part xc7z020clg400-2 -force
add_files [glob $example/*_v1_0/*.v]
add_files -fileset sim_1 [list $example/tests/chirp_generator_tb.sv $example/tests/adc_capture_tb.sv]
foreach top {chirp_generator_tb adc_capture_tb} {
    set_property top $top [get_filesets sim_1]
    set_property xsim.simulate.runtime 0ns [get_filesets sim_1]
    launch_simulation
    run all
    if {[get_value -radix unsigned /$top/passed] != 1} { error "$top failed" }
    close_sim
}
close_project
