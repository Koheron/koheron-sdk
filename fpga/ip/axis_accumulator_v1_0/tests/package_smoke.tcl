set root [file normalize [file join [file dirname [info script]] ../../../..]]
set repo [file normalize [lindex $argv 0]]
set out [file normalize [lindex $argv 1]]
if {![file exists $repo/axis_accumulator_v1_0/component.xml]} {error {Generate the package first}}
if {$out eq $root} {error {Specify an output directory}}
create_project -force accumulator_catalog $out -part xc7z020clg400-2
set_property ip_repo_paths $repo [current_project]
update_ip_catalog
create_bd_design system
source $root/fpga/lib/utilities.tcl
source $root/fpga/lib/axis_accumulator.tcl
foreach {name dir width type} {
    clk I 1 clk resetn I 1 rst data I 32 data valid I 1 data last I 1 data
    sum O 32 data addr O 32 data cycle O 32 data wen O 4 data
} {
    if {$type ne "data"} {
        create_bd_port -dir $dir -type $type $name
    } elseif {$width == 1} {
        create_bd_port -dir $dir $name
    } else {
        create_bd_port -dir $dir -from [expr $width-1] -to 0 $name
    }
}
set_property CONFIG.FREQ_HZ 250000000 [get_bd_ports clk]
set_property CONFIG.ASSOCIATED_RESET resetn [get_bd_ports clk]
set_property CONFIG.POLARITY ACTIVE_LOW [get_bd_ports resetn]
axis_accumulator::create accum 64 3
foreach {port pin} {
    clk clk resetn resetn data s_axis_tdata valid s_axis_tvalid last s_axis_tlast
    sum m_axis_tdata addr addr_out cycle cycle_index wen wen
} {connect_port_pin $port accum/$pin}
validate_bd_design
save_bd_design
set_property synth_checkpoint_mode None [get_files system.bd]
generate_target all [get_files system.bd]
add_files [make_wrapper -files [get_files system.bd] -top]
add_files -fileset sim_1 $root/fpga/ip/axis_accumulator_v1_0/tests/legacy_tb.sv
set_property top legacy_tb [get_filesets sim_1]
set_property xsim.simulate.runtime 0ns [get_filesets sim_1]
set_property xsim.elaborate.debug_level all [get_filesets sim_1]
launch_simulation
run all
set passed [get_value -radix bin [get_objects /*/passed]]
close_sim
if {$passed ne "1"} {error {Packaged accumulator legacy regression failed}}
set_property top system_wrapper [current_fileset]
update_compile_order -fileset sources_1
synth_design -top system_wrapper -mode out_of_context -part xc7z020clg400-2
if {[llength [get_cells -hier -filter {IS_BLACKBOX == 1}]]} {error {Unresolved black boxes in packaged accumulator}}
report_utilization -file $out/utilization.rpt
write_checkpoint -force $out/accumulator.dcp
close_project
