
set instrument_name [lindex $argv 0]

set instrument_path [lindex $argv 1]

set part_name [lindex $argv 2]

file delete -force tmp/$instrument_name.cache tmp/$instrument_name.hw tmp/$instrument_name.srcs tmp/$instrument_name.runs tmp/$instrument_name.xpr tmp/$instrument_name.sim

create_instrument -part $part_name $instrument_name tmp

set_property IP_REPO_PATHS tmp/cores [current_instrument]
update_ip_catalog

set bd_path tmp/$instrument_name.srcs/sources_1/bd/system

create_bd_design system

set lib fpga/lib
source $lib/utilities.tcl

source $instrument_path/$instrument_name/block_design.tcl

rename cell {}

generate_target all [get_files $bd_path/system.bd]
make_wrapper -files [get_files $bd_path/system.bd] -top

add_files -norecurse $bd_path/hdl/system_wrapper.v

add_files -norecurse $instrument_path/$instrument_name/test_bench.v

set_property -name {xsim.simulate.runtime} -value {100000ns} -objects [current_fileset -simset]

update_compile_order -fileset sources_1
update_compile_order -fileset sim_1
launch_simulation
