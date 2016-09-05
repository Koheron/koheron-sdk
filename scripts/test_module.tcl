
set project_name [lindex $argv 0]

set project_path [lindex $argv 1]

set part_name [lindex $argv 2]

file delete -force tmp/$project_name.cache tmp/$project_name.hw tmp/$project_name.srcs tmp/$project_name.runs tmp/$project_name.xpr tmp/$project_name.sim

create_project -part $part_name $project_name tmp

set_property IP_REPO_PATHS tmp/cores [current_project]
update_ip_catalog

set bd_path tmp/$project_name.srcs/sources_1/bd/system

create_bd_design system

set lib fpga/tcl
source $lib/utilities.tcl

source $project_path/$project_name/block_design.tcl

rename cell {}

generate_target all [get_files $bd_path/system.bd]
make_wrapper -files [get_files $bd_path/system.bd] -top

add_files -norecurse $bd_path/hdl/system_wrapper.v

add_files -norecurse $project_path/$project_name/test_bench.v

set_property -name {xsim.simulate.runtime} -value {100000ns} -objects [current_fileset -simset]

update_compile_order -fileset sources_1
update_compile_order -fileset sim_1
launch_simulation
