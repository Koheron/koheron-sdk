set sdk_path [lindex $argv 0]
set project_name [lindex $argv 1]
set project_path [lindex $argv 2]
set part [lindex $argv 3]
set output_path [lindex $argv 4]

set $sdk_path/fpga/lib $sdk_path/fpga/lib

file delete -force \
  $output_path/$project_name.cache \
  $output_path/$project_name.hw \
  $output_path/$project_name.srcs \
  $output_path/$project_name.runs \
  $output_path/$project_name.xpr \
  $output_path/$project_name.sim

create_project -force -part $part $project_name $output_path

set_property IP_REPO_PATHS $output_path/../cores [current_project]
update_ip_catalog -rebuild -scan_changes

set bd_path $output_path/$project_name.srcs/sources_1/bd/system

create_bd_design system

source $sdk_path/fpga/lib/utilities.tcl

source $project_path/block_design.tcl

rename cell {}

generate_target all [get_files $bd_path/system.bd]
make_wrapper -files [get_files $bd_path/system.bd] -top

add_files -norecurse $bd_path/hdl/system_wrapper.v

add_files -norecurse [glob -nocomplain $project_path/test_bench.v*] -fileset sim_1

set_property -name {xsim.simulate.runtime} -value {100000ns} -objects [current_fileset -simset]

update_compile_order -fileset sources_1
update_compile_order -fileset sim_1
launch_simulation
