
set core_name [lindex $argv 0]

set part_name [lindex $argv 1]

set elements [split $core_name _]
set instrument_name test_[join [lrange $elements 0 end-2] _]
set version [string trimleft [join [lrange $elements end-1 end] .] v]

set cores_dir fpga/cores

file delete -force tmp/cores/$instrument_name tmp/cores/$instrument_name.ip_user_files tmp/cores/$instrument_name.sim tmp/cores/$instrument_name.runs tmp/cores/$instrument_name.cache tmp/cores/$instrument_name.hw tmp/cores/$instrument_name.xpr

create_instrument -part $part_name $instrument_name tmp/cores

add_files -norecurse [glob $cores_dir/$core_name/*.v]

set_property -name {xsim.simulate.runtime} -value {100000ns} -objects [current_fileset -simset]

update_compile_order -fileset sources_1
update_compile_order -fileset sim_1
launch_simulation
