set root [file normalize [file join [file dirname [info script]] ..]]
create_project phase_accumulator_tests [file normalize tmp/dds-pm-phase-accumulator-sim] -part xc7z020clg400-2 -force
add_files [list $root/pm_phase_wrap.v $root/pm_waveform.v]
add_files -fileset sim_1 $root/tests/phase_accumulator_tb.sv
set_property top phase_accumulator_tb [get_filesets sim_1]
set_property xsim.simulate.runtime 0ns [get_filesets sim_1]
launch_simulation
run all
if {[get_value -radix unsigned /phase_accumulator_tb/passed] != 1} {error "Phase accumulator test failed"}
close_sim
close_project
