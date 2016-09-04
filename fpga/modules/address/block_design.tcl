source tmp/$project_name.config.tcl
source $project_path/$project_name/address.tcl

set module address

address::create $module $config::bram_addr_width
address::pins create_bd_port $config::bram_addr_width
connect_ports $module
