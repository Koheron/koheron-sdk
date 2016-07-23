
set project_name [lindex $argv 0]

set part_name [lindex $argv 1]

set board_name [lindex $argv 2]

set cfg boards/$board_name/config
set lib fpga/tcl

file delete -force tmp/$project_name.cache tmp/$project_name.hw tmp/$project_name.srcs tmp/$project_name.runs tmp/$project_name.xpr tmp/$project_name.sim

create_project -part $part_name $project_name tmp

set_property IP_REPO_PATHS tmp/cores [current_project]
update_ip_catalog -rebuild -scan_changes

set bd_path tmp/$project_name.srcs/sources_1/bd/system

create_bd_design system

source $cfg/ports.tcl
source $lib/utilities.tcl
source tmp/$project_name.config.tcl
source projects/$project_name/block_design.tcl
