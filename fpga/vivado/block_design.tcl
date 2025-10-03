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

puts "==============================="
puts "BUILDING PROJECT: $project_name"
puts "==============================="
puts "PROJECT_PATH = $project_path"
puts "BOARD_PATH = $board_path"
puts ""

file delete -force \
  $output_path/$project_name.cache \
  $output_path/$project_name.hw \
  $output_path/$project_name.srcs \
  $output_path/$project_name.runs \
  $output_path/$project_name.xpr \
  $output_path/$project_name.sim

set_param board.repoPaths {}

create_project -force -part $part $project_name $output_path

catch { unset_property board_part [current_project] }

set_property IP_REPO_PATHS [list $::env(TMP_CORES_PATH)] [current_project]
#update_ip_catalog -rebuild -scan_changes
update_ip_catalog

set_msg_config -string {Only lower order bits will be connected} -suppress
set_msg_config -string {is being overridden by the user} -suppress
set_msg_config -string {It is recommended to use inline hdl version} -suppress
set_msg_config -string {has been set to manual on the GUI} -suppress


set bd_path $output_path/$project_name.srcs/sources_1/bd/system

create_bd_design system

source $sdk_path/fpga/lib/utilities.tcl

source $output_path/memory.tcl

puts "BD TCL"
puts $::env(BD_TCL)

source $::env(BD_TCL)
