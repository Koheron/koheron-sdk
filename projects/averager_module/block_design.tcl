source projects/$project_name/config.tcl
source projects/$project_name/averager.tcl

set module averager

add_averager_module $module $config::bram_addr_width -input_type fix_$config::adc_width

set fast_count_width $config::bram_addr_width
set slow_count_width [expr 32 - $config::bram_addr_width]

create_bd_port -dir I -type clk clk
connect_bd_net [get_bd_pins /$module/clk] [get_bd_ports clk]

create_bd_port -dir I avg_on
connect_bd_net [get_bd_pins /$module/avg_on] [get_bd_ports avg_on]

create_bd_port -dir I -from [expr $config::adc_width - 1] -to 0 din
connect_bd_net [get_bd_pins /$module/din] [get_bd_ports din]

create_bd_port -dir I tvalid
connect_bd_net [get_bd_pins /$module/tvalid] [get_bd_ports tvalid]

create_bd_port -dir I restart
connect_bd_net [get_bd_pins /$module/restart] [get_bd_ports restart]

create_bd_port -dir I -from [expr $fast_count_width - 1] -to 0 period
connect_bd_net [get_bd_pins /$module/period] [get_bd_ports period]

create_bd_port -dir I -from [expr $fast_count_width - 1] -to 0 threshold
connect_bd_net [get_bd_pins /$module/threshold] [get_bd_ports threshold]

create_bd_port -dir I -from [expr $slow_count_width - 1] -to 0 n_avg_min
connect_bd_net [get_bd_pins /$module/n_avg_min] [get_bd_ports n_avg_min]

create_bd_port -dir O -from 3 -to 0 wen
connect_bd_net [get_bd_pins /$module/wen] [get_bd_ports wen]

create_bd_port -dir O -from [expr $slow_count_width - 1] -to 0 n_avg
connect_bd_net [get_bd_pins /$module/n_avg] [get_bd_ports n_avg]

create_bd_port -dir O -from [expr $fast_count_width + 1] -to 0 addr
connect_bd_net [get_bd_pins /$module/addr] [get_bd_ports addr]

create_bd_port -dir O -from 31 -to 0 dout
connect_bd_net [get_bd_pins /$module/dout] [get_bd_ports dout]

create_bd_port -dir O ready
connect_bd_net [get_bd_pins /$module/ready] [get_bd_ports ready]

create_bd_port -dir O avg_on_out
connect_bd_net [get_bd_pins /$module/avg_on_out] [get_bd_ports avg_on_out]
