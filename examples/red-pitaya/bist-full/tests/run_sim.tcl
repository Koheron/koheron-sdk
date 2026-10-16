set project_path [file normalize [file join [file dirname [info script]] ..]]
set output [file normalize [lindex $argv 0]]
file mkdir $output
cd $output
exec xvlog $project_path/full_path_v1_0/full_path.v
exec xvlog -sv $project_path/tests/path_tb.sv
exec xelab path_tb -s full_sim
set result [exec xsim full_sim -runall]
puts $result
if {[string first {PASS full datapath:} $result] < 0 || [string first {Fatal:} $result] >= 0} {error {datapath simulation failed}}
exec xvlog $project_path/ddr_tester_v1_0/ddr_tester.v
exec xvlog -sv $project_path/tests/ddr_tb.sv
exec xelab ddr_tb -s ddr_sim
set result [exec xsim ddr_sim -runall]
puts $result
if {[string first {PASS DDR master:} $result] < 0 || [string first {Fatal:} $result] >= 0} {error {DDR simulation failed}}
