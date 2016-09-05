
set core_name [lindex $argv 0]

set part_name [lindex $argv 1]

set elements [split $core_name _]
set project_name test_[join [lrange $elements 0 end-2] _]
set version [string trimleft [join [lrange $elements end-1 end] .] v]

set cores_dir fpga/cores

file delete -force tmp/cores/$project_name tmp/cores/$project_name.ip_user_files tmp/cores/$project_name.sim tmp/cores/$project_name.runs tmp/cores/$project_name.cache tmp/cores/$project_name.hw tmp/cores/$project_name.xpr

create_project -part $part_name $project_name tmp/cores

add_files -norecurse [glob $cores_dir/$core_name/*.v]

set_property -name {xsim.simulate.runtime} -value {100000ns} -objects [current_fileset -simset]

update_compile_order -fileset sources_1
update_compile_order -fileset sim_1
launch_simulation
