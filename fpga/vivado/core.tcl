
set core_path [lindex $argv 0]
set part [lindex $argv 1]
set output_path [lindex $argv 2]

set core_name [lindex [split $core_path /] end]

set elements [split $core_name _]
set project_name [join [lrange $elements 0 end-2] _]
set version [string trimleft [join [lrange $elements end-1 end] .] v]

file delete -force $output_path/$core_name $output_path/$project_name.cache $output_path/$project_name.hw $output_path/$project_name.xpr

create_project -part $part $project_name $output_path

add_files -norecurse [glob $core_path/*.v*]

# Remove testbench files
set testbench_files [glob -nocomplain $core_path/*_tb.v*]
if {[llength testbench_files] > 0} {
  remove_files $testbench_files
}

ipx::package_project -import_files -root_dir $output_path/$core_name

set core [ipx::current_core]

set_property VERSION $version $core
set_property NAME $project_name $core
set_property LIBRARY {user} $core
set_property SUPPORTED_FAMILIES {zynq Production} $core

proc core_parameter {name display_name description} {
  set core [ipx::current_core]

  set parameter [ipx::get_user_parameters $name -of_objects $core]
  set_property DISPLAY_NAME $display_name $parameter
  set_property DESCRIPTION $description $parameter

  set parameter [ipgui::get_guiparamspec -name $name -component $core]
  set_property DISPLAY_NAME $display_name $parameter
  set_property TOOLTIP $description $parameter
}

source $core_path/core_config.tcl

rename core_parameter {}

ipx::create_xgui_files $core
ipx::update_checksums $core
ipx::save_core $core

close_project
