set display_name {Picoblaze}

set core [ipx::current_core]

set_property DISPLAY_NAME $display_name $core
set_property DESCRIPTION $display_name $core

set_property VENDOR {xilinx} $core
set_property VENDOR_DISPLAY_NAME {Xilinx} $core
set_property COMPANY_URL {http://www.xilinx.com} $core
