source projects/$project_name/config.tcl
source lib/peak_detector.tcl

set module peak_detector
add_peak_detector $module $config::wfm_width

create_bd_port -dir I -type clk clk
connect_bd_net [get_bd_pins /$module/clk] [get_bd_ports clk]

create_bd_port -dir I -from 31 -to 0 din
connect_bd_net [get_bd_pins /$module/din] [get_bd_ports din]

create_bd_port -dir I -from [expr $config::wfm_width - 1] -to 0 address_low
connect_bd_net [get_bd_pins /$module/address_low] [get_bd_ports address_low]

create_bd_port -dir I -from [expr $config::wfm_width - 1] -to 0 address_high
connect_bd_net [get_bd_pins /$module/address_high] [get_bd_ports address_high]

create_bd_port -dir I -from [expr $config::wfm_width - 1] -to 0 address_reset
connect_bd_net [get_bd_pins /$module/address_reset] [get_bd_ports address_reset]

create_bd_port -dir I s_axis_tvalid
connect_bd_net [get_bd_pins /$module/s_axis_tvalid] [get_bd_ports s_axis_tvalid]

create_bd_port -dir O -from [expr $config::wfm_width - 1] -to 0 address_out
connect_bd_net [get_bd_pins /$module/address_out] [get_bd_ports address_out]

create_bd_port -dir O -from 31 -to 0 maximum_out
connect_bd_net [get_bd_pins /$module/maximum_out] [get_bd_ports maximum_out]

create_bd_port -dir O m_axis_tvalid
connect_bd_net [get_bd_pins /$module/m_axis_tvalid] [get_bd_ports m_axis_tvalid]
