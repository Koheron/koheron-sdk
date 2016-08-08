source projects/$project_name/config.tcl
source projects/$project_name/peak_detector.tcl

set module peak_detector
peak_detector::create $module $config::wfm_width
peak_detector::pins create_bd_port $config::wfm_width

connect_ports $module
