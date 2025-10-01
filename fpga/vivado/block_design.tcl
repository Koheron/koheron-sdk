set prefix [lindex $argv 0]
set sdk_path $::env(SDK_PATH)
set project_name $::env(NAME)
set project_path $::env(PROJECT_PATH)
set part $::env(PART)
set board_path $::env(BOARD_PATH)
set mode $::env(MODE)
set output_path $::env(TMP_FPGA_PATH)
set python $::env(VENV)/bin/python3

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

set_property IP_REPO_PATHS [list $::env(TMP_CORES_PATH)] [current_project]
#update_ip_catalog -rebuild -scan_changes
update_ip_catalog

set bd_path $output_path/$project_name.srcs/sources_1/bd/system

create_bd_design system

source $sdk_path/fpga/lib/utilities.tcl

source $output_path/memory.tcl

puts "BD TCL"
puts $::env(BD_TCL)

source $::env(BD_TCL)
