source projects/$project_name/config.tcl
source projects/$project_name/spectrum.tcl

set module spectrum_module
set n_pts_fft [expr 2**$config::bram_addr_width]

add_spectrum $module $n_pts_fft $config::adc_width

create_bd_port -dir I -type clk clk
create_bd_port -dir I -from [expr $config::adc_width - 1] -to 0 adc1
create_bd_port -dir I -from [expr $config::adc_width - 1] -to 0 adc2
create_bd_port -dir I -from 31 -to 0 cfg_sub
create_bd_port -dir I -from 31 -to 0 cfg_fft
create_bd_port -dir I -from 31 -to 0 demod_data
create_bd_port -dir I tvalid

create_bd_port -dir O -from 31 -to 0 m_axis_result_tdata
create_bd_port -dir O m_axis_result_tvalid

connect_ports $module
