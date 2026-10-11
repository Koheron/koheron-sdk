set core [file normalize [file join [file dirname [info script]] ..]]
create_project vna_capture_tests [file normalize tmp/vna-capture-sim] -part xc7z020clg400-2 -force
add_files $core/vna_capture.v
add_files -fileset sim_1 $core/tests/vna_capture_tb.sv
set_property top vna_capture_tb [get_filesets sim_1]
launch_simulation
run all
if {[get_value /vna_capture_tb/passed] ne "1"} {error "VNA capture test did not pass"}
close_sim
