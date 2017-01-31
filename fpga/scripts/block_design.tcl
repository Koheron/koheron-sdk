
set project_name [lindex $argv 0]

set project_path [lindex $argv 1]

set part_name [lindex $argv 2]

set board_name [lindex $argv 3]

# Add optional prefix to the project name
set prefix [lindex $argv 4]
set prefixed_project_name $prefix$project_name

file delete -force \
  tmp/$prefixed_project_name.cache \
  tmp/$prefixed_project_name.hw \
  tmp/$prefixed_project_name.srcs \
  tmp/$prefixed_project_name.runs \
  tmp/$prefixed_project_name.xpr \
  tmp/$prefixed_project_name.sim

create_project -part $part_name $prefixed_project_name tmp

set_property IP_REPO_PATHS tmp/cores [current_project]
update_ip_catalog -rebuild -scan_changes

set bd_path tmp/$prefixed_project_name.srcs/sources_1/bd/system

create_bd_design system

source fpga/lib/utilities.tcl

# Source configuration file generated from the template scripts/templates/config.tcl
source tmp/$project_name.config.tcl

source $project_path/block_design.tcl
