set project_path [file normalize [file join [file dirname [info script]] ..]]
set output [file normalize [lindex $argv 0]]
file mkdir $output
cd $output
exec xvlog $project_path/full_path_v1_0/full_path.v
exec xvlog -sv $project_path/tests/path_tb.sv
exec xelab path_tb -s full_sim
puts [exec xsim full_sim -runall]
