
set instrument_name [lindex $argv 0]

set instrument_path [lindex $argv 1]

set part_name [lindex $argv 2]

set board_name [lindex $argv 3]

# Add optional prefix to the instrument name
set prefix [lindex $argv 4]
set prefixed_instrument_name $prefix$instrument_name

file delete -force \
  tmp/$prefixed_instrument_name.cache \
  tmp/$prefixed_instrument_name.hw \
  tmp/$prefixed_instrument_name.srcs \
  tmp/$prefixed_instrument_name.runs \
  tmp/$prefixed_instrument_name.xpr \
  tmp/$prefixed_instrument_name.sim

create_project -part $part_name $prefixed_instrument_name tmp

set_property IP_REPO_PATHS tmp/cores [current_instrument]
update_ip_catalog -rebuild -scan_changes

set bd_path tmp/$prefixed_instrument_name.srcs/sources_1/bd/system

create_bd_design system

set lib fpga/lib
source $lib/utilities.tcl

# Source configuration file generated from the template scripts/templates/config.tcl
source tmp/$instrument_name.config.tcl

source $instrument_path/$instrument_name/block_design.tcl
