set root [file normalize [file join [file dirname [info script]] ../../../..]]
set core_path $root/fpga/ip/axis_accumulator_v1_0
set output_path [file normalize [lindex $argv 0]]
if {$output_path eq $root} {error {Specify a simulation output directory}}
create_project -force accumulator_sim $output_path -part xc7z020clg400-2
add_files [glob $core_path/*.v]
source $core_path/package_ip.tcl
generate_target all [get_ips axis_accumulator_add]
add_files -fileset sim_1 $core_path/tests/stream_tb.sv
set_property top stream_tb [get_filesets sim_1]
set_property xsim.simulate.runtime 0ns [get_filesets sim_1]
set_property xsim.elaborate.debug_level all [get_filesets sim_1]
update_compile_order -fileset sources_1
update_compile_order -fileset sim_1
# Bins, frames per sum, pauses/backpressure, TLAST validation.
set profiles {
    {64 3 0 1} {64 3 1 1}
    {16 1 1 1} {64 2 1 1}
    {3 3 1 1} {1 1 1 1} {1 3 1 1}
    {64 3 0 0} {64 3 1 0}
    {10 3 1 1} {11 3 0 1} {8192 3 0 1}
}
foreach profile $profiles {
    lassign $profile bins frames stalls check_last
    set_property generic "FRAME_LENGTH=$bins N_FRAMES=$frames STALLS=$stalls CHECK_TLAST=$check_last" [get_filesets sim_1]
    launch_simulation
    run all
    set passed [get_value -radix bin [get_objects /*/passed]]
    close_sim
    if {$passed ne "1"} {error "Accumulator regression failed: $profile"}
}
close_project
