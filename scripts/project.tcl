
set project_name [lindex $argv 0]

set part_name [lindex $argv 1]

set board_name [lindex $argv 2]

set cfg boards/$board_name/config


file delete -force tmp/$project_name.cache tmp/$project_name.hw tmp/$project_name.srcs tmp/$project_name.runs tmp/$project_name.xpr tmp/$project_name.sim

create_project -part $part_name $project_name tmp

set_property IP_REPO_PATHS tmp/cores [current_project]

set bd_path tmp/$project_name.srcs/sources_1/bd/system

create_bd_design system

source $cfg/ports.tcl
set lib fpga/tcl
source $lib/utilities.tcl

source projects/$project_name/block_design.tcl

rename cell {}
rename module {}
rename properties {}

generate_target all [get_files $bd_path/system.bd]
make_wrapper -files [get_files $bd_path/system.bd] -top

add_files -norecurse $bd_path/hdl/system_wrapper.v

set files [glob -nocomplain projects/$project_name/*.v projects/$project_name/*.sv]
if {[llength $files] > 0} {
  add_files -norecurse $files
}

set files [glob -nocomplain tmp/$project_name.xdc/*.xdc]
if {[llength $files] > 0} {
  add_files -norecurse -fileset constrs_1 $files
}

set_property VERILOG_DEFINE {TOOL_VIVADO} [current_fileset]

set_property STRATEGY Flow_PerfOptimized_High [get_runs synth_1]
set_property STRATEGY Performance_NetDelay_high [get_runs impl_1]

close_project
