source projects/$project_name/config.tcl
source projects/$project_name/averager.tcl

set module averager

add_averager_module $module $config::bram_addr_width -input_type fix_$config::adc_width

set fast_count_width $config::bram_addr_width
set slow_count_width [expr 32 - $config::bram_addr_width]

create_bd_port -dir I -type clk clk
create_bd_port -dir I avg_on
create_bd_port -dir I -from [expr $config::adc_width - 1] -to 0 din
create_bd_port -dir I tvalid
create_bd_port -dir I restart
create_bd_port -dir I -from [expr $fast_count_width - 1] -to 0 period
create_bd_port -dir I -from [expr $fast_count_width - 1] -to 0 threshold
create_bd_port -dir I -from [expr $slow_count_width - 1] -to 0 n_avg_min

create_bd_port -dir O -from 3 -to 0 wen
create_bd_port -dir O -from [expr $slow_count_width - 1] -to 0 n_avg
create_bd_port -dir O -from [expr $fast_count_width + 1] -to 0 addr
create_bd_port -dir O -from 31 -to 0 dout
create_bd_port -dir O ready
create_bd_port -dir O avg_on_out

connect_ports $module
