source projects/$project_name/config.tcl
source lib/spectrum.tcl

set module spectrum_module
set n_pts_fft [expr 2**$config::bram_addr_width]

add_spectrum $module $n_pts_fft $config::adc_width

create_bd_port -dir I -type clk clk
connect_bd_net [get_bd_pins /$module/clk] [get_bd_ports clk]

create_bd_port -dir I -from [expr $config::adc_width - 1] -to 0 adc1
connect_bd_net [get_bd_pins /$module/adc1] [get_bd_ports adc1]

create_bd_port -dir I -from [expr $config::adc_width - 1] -to 0 adc2
connect_bd_net [get_bd_pins /$module/adc2] [get_bd_ports adc2]

create_bd_port -dir I -from 31 -to 0 cfg_sub
connect_bd_net [get_bd_pins /$module/cfg_sub] [get_bd_ports cfg_sub]

create_bd_port -dir I -from 31 -to 0 cfg_fft
connect_bd_net [get_bd_pins /$module/cfg_fft] [get_bd_ports cfg_fft]

create_bd_port -dir I -from 31 -to 0 demod_data
connect_bd_net [get_bd_pins /$module/demod_data] [get_bd_ports demod_data]

create_bd_port -dir I tvalid
connect_bd_net [get_bd_pins /$module/tvalid] [get_bd_ports tvalid]

create_bd_port -dir O -from 31 -to 0 m_axis_result_tdata
connect_bd_net [get_bd_pins /$module/m_axis_result_tdata] [get_bd_ports m_axis_result_tdata]

create_bd_port -dir O m_axis_result_tvalid
connect_bd_net [get_bd_pins /$module/m_axis_result_tvalid] [get_bd_ports m_axis_result_tvalid]
