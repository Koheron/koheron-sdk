source $output_path/config.tcl
source $project_path/spectrum.tcl

set module spectrum_module
set n_pts_fft [expr 2**$config::bram_addr_width]

spectrum::create $module $n_pts_fft $config::adc_width
spectrum::pins create_bd_port $config::adc_width
connect_ports $module
