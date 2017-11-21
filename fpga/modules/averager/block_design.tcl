source $output_path/config.tcl
source $project_path/averager.tcl

set module averager
set fast_count_width $config::bram_addr_width
set slow_count_width [expr 32 - $config::bram_addr_width]

averager::create $module $config::bram_addr_width -input_type fix_$config::adc_width
averager::pins create_bd_port $fast_count_width $slow_count_width $config::adc_width
connect_ports $module
