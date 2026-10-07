if {$argc != 1} {error "Expected full DPLL project"}
set root [file normalize [file join [file dirname [info script]] ../../../..]]
set project [file normalize [lindex $argv 0]]
set out $root/tmp/tests/alpha250-dpll/extractor-latency
set inputs {}
foreach name {system_complex_mult_0 system_cordic_0} {
    lappend inputs [file rootname $project].srcs/sources_1/bd/system/ip/$name/$name.xci
}
create_project -force extractor_latency $out -part xc7z020clg400-2
foreach input $inputs {import_ip -files $input}
generate_target simulation [get_ips]
add_files -fileset sim_1 $root/examples/alpha250/dpll/tests/test_extractor_latency_tb.sv
set_property top test_extractor_latency_tb [get_filesets sim_1]
set_property xsim.simulate.runtime all [get_filesets sim_1]
launch_simulation
close_sim
source $root/examples/alpha250/dpll/tests/check_simulation.tcl
check_simulation $out/extractor_latency.sim/sim_1/behav/xsim/simulate.log {Extractor latency checks passed:}
close_project
