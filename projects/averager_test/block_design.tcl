source projects/$project_name/config.tcl
source lib/averager.tcl

add_averager_module averager $config::bram_addr_width -input_type fix_$config::adc_width

set fast_count_width $config::bram_addr_width
set slow_count_width [expr 32 - $config::bram_addr_width]

create_bd_port -dir I -type clk clk
connect_bd_net [get_bd_pins /averager/clk] [get_bd_ports clk]

create_bd_port -dir I avg_off
connect_bd_net [get_bd_pins /averager/avg_off] [get_bd_ports avg_off]

create_bd_port -dir I -from [expr $config::adc_width - 1] -to 0 din
connect_bd_net [get_bd_pins /averager/din] [get_bd_ports din]

create_bd_port -dir I tvalid
connect_bd_net [get_bd_pins /averager/tvalid] [get_bd_ports tvalid]

create_bd_port -dir I restart
connect_bd_net [get_bd_pins /averager/restart] [get_bd_ports restart]

create_bd_port -dir I -from [expr $fast_count_width - 1] -to 0 period
connect_bd_net [get_bd_pins /averager/period] [get_bd_ports period]

create_bd_port -dir I -from [expr $fast_count_width - 1] -to 0 threshold
connect_bd_net [get_bd_pins /averager/threshold] [get_bd_ports threshold]

create_bd_port -dir O -from 3 -to 0 wen
connect_bd_net [get_bd_pins /averager/wen] [get_bd_ports wen]

create_bd_port -dir O -from [expr $slow_count_width - 1] -to 0 n_avg
connect_bd_net [get_bd_pins /averager/n_avg] [get_bd_ports n_avg]

create_bd_port -dir O -from [expr $fast_count_width + 1] -to 0 addr
connect_bd_net [get_bd_pins /averager/addr] [get_bd_ports addr]

create_bd_port -dir O -from 31 -to 0 dout
connect_bd_net [get_bd_pins /averager/dout] [get_bd_ports dout]

create_bd_port -dir O ready
connect_bd_net [get_bd_pins /averager/ready] [get_bd_ports ready]

