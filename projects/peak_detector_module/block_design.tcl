source projects/$project_name/config.tcl
source projects/$project_name/peak_detector.tcl

set module peak_detector
add_peak_detector $module $config::wfm_width

create_bd_port -dir I -type clk clk
create_bd_port -dir I -from 31 -to 0 din
create_bd_port -dir I -from [expr $config::wfm_width - 1] -to 0 address_low
create_bd_port -dir I -from [expr $config::wfm_width - 1] -to 0 address_high
create_bd_port -dir I -from [expr $config::wfm_width - 1] -to 0 address_reset
create_bd_port -dir I s_axis_tvalid

create_bd_port -dir O -from [expr $config::wfm_width - 1] -to 0 address_out
create_bd_port -dir O -from 31 -to 0 maximum_out
create_bd_port -dir O m_axis_tvalid

connect_ports $module
