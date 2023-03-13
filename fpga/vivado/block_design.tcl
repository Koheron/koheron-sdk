set sdk_path [lindex $argv 0]
set project_name [lindex $argv 1]
set project_path [lindex $argv 2]
set part [lindex $argv 3]
set board_path [lindex $argv 4]
set mode [lindex $argv 5]
set output_path [lindex $argv 6]
set xdc_filename [lindex $argv 7]
set python [lindex $argv 8]
set prefix [lindex $argv 9]

# Add optional prefix to the project name
if {$prefix == "block_design_"} {
  set project_name $prefix$project_name
} else {
  set project_name $project_name
}

puts $project_name

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

source $output_path/config.tcl

source $project_path/block_design.tcl
