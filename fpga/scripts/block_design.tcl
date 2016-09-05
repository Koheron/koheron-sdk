
set name [lindex $argv 0]

set project_path [lindex $argv 1]

set part_name [lindex $argv 2]

set board_name [lindex $argv 3]

set prefix [lindex $argv 4]

set cfg boards/$board_name/config
set lib fpga/lib

set project_name $prefix$name

file delete -force tmp/$project_name.cache tmp/$project_name.hw tmp/$project_name.srcs tmp/$project_name.runs tmp/$project_name.xpr tmp/$project_name.sim

create_project -part $part_name $project_name tmp

set_property IP_REPO_PATHS tmp/cores [current_project]
update_ip_catalog -rebuild -scan_changes

set bd_path tmp/$project_name.srcs/sources_1/bd/system

create_bd_design system

source $cfg/ports.tcl
source $lib/utilities.tcl
source tmp/$name.config.tcl
source $project_path/$name/block_design.tcl
