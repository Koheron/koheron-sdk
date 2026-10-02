set root [file normalize [file join [file dirname [info script]] ..]]
source $root/integration.tcl
set options [dds_pm::defaults {}]
create_project pm_tests [file normalize tmp/dds-pm-sim] -part xc7z020clg400-2 -force
add_files [glob $root/*.v]
foreach {name kind} {pm_carrier carrier pm_modulation modulation} {
    create_ip -name dds_compiler -vendor xilinx.com -library ip -version 6.0 -module_name $name
    set_property -dict [dds_pm::${kind}_properties $options 250000000] [get_ips $name]
    if {$kind eq "carrier"} {
        set_property CONFIG.Has_Phase_Out true [get_ips $name]
    }
    generate_target simulation [get_ips $name]
}
add_files -fileset sim_1 $root/tests/pm_tb.sv
set_property top pm_tb [get_filesets sim_1]
set_property xsim.simulate.runtime 0ns [get_filesets sim_1]
launch_simulation
run all
if {[get_value -radix unsigned /pm_tb/passed] != 1} { error "PM integration test failed" }
close_sim
close_project
