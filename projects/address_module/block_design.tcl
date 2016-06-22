source projects/$project_name/config.tcl
source projects/$project_name/address.tcl

set module address
add_address_module $module $config::bram_addr_width

create_bd_port -dir I -type clk clk
create_bd_port -dir I -from 31 -to 0 cfg
create_bd_port -dir I -from 31 -to 0 period

create_bd_port -dir O -from [expr $config::bram_addr_width+1] -to 0 addr
create_bd_port -dir O restart
create_bd_port -dir O tvalid

connect_ports $module
