
set core_name [lindex $argv 0]

set part [lindex $argv 1]

set output_path [lindex $argv 2]

set elements [split $core_name _]
set project_name test_[join [lrange $elements 0 end-2] _]
set version [string trimleft [join [lrange $elements end-1 end] .] v]

file delete -force \
  $output_path/cores/$project_name \
  $output_path/cores/$project_name.ip_user_files \
  $output_path/cores/$project_name.sim \
  $output_path/cores/$project_name.runs \
  $output_path/cores/$project_name.cache \
  $output_path/cores/$project_name.hw \
  $output_path/cores/$project_name.xpr

create_project -part $part $project_name $output_path/cores

add_files -norecurse [glob $core_name/*.v*]

set_property -name {xsim.simulate.runtime} -value {100000ns} -objects [current_fileset -simset]

update_compile_order -fileset sources_1
update_compile_order -fileset sim_1
launch_simulation
