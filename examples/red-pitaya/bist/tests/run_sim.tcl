set project_path [file normalize [file join [file dirname [info script]] ..]]
set output [file normalize [lindex $argv 0]]
file mkdir $output
cd $output
exec xvlog $project_path/bist_uart_v1_0/bist_uart.v
exec xvlog -sv $project_path/tests/uart_tb.sv
exec xelab uart_tb -s uart_sim
puts [exec xsim uart_sim -runall]

exec xvlog $project_path/bist_path_v1_0/bist_path.v
exec xvlog -sv $project_path/tests/path_tb.sv
exec xelab path_tb -s path_sim
puts [exec xsim path_sim -runall]
