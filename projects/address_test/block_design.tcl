source projects/$project_name/config.tcl
source lib/address.tcl

set module address

add_address_module $module $config::bram_addr_width

create_bd_port -dir I -type clk clk
connect_bd_net [get_bd_pins /$module/clk] [get_bd_ports clk]

create_bd_port -dir I -from 31 -to 0 cfg
connect_bd_net [get_bd_pins /$module/cfg] [get_bd_ports cfg]

create_bd_port -dir I -from 31 -to 0 period
connect_bd_net [get_bd_pins /$module/period] [get_bd_ports period]

create_bd_port -dir O -from [expr $config::bram_addr_width+1] -to 0 addr
connect_bd_net [get_bd_pins /$module/addr] [get_bd_ports addr]

create_bd_port -dir O restart
connect_bd_net [get_bd_pins /$module/restart] [get_bd_ports restart]

create_bd_port -dir O tvalid
connect_bd_net [get_bd_pins /$module/tvalid] [get_bd_ports tvalid]
