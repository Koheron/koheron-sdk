# core.tcl (incremental & -j safe)

# Args
set core_path   [lindex $argv 0]
set part        [lindex $argv 1]
set output_path [lindex $argv 2]

# Names/paths
set core_name    [lindex [split $core_path /] end]
set elements     [split $core_name _]
set project_name [join [lrange $elements 0 end-2] _]
set version      [string trimleft [join [lrange $elements end-1 end] .] v]
set out_dir      $output_path/$core_name
set comp_xml     $out_dir/component.xml

# Collect inputs (RTL + config)
set rtl [concat \
  [glob -nocomplain $core_path/*.v] \
  [glob -nocomplain $core_path/*.sv] \
  [glob -nocomplain $core_path/*.vh] \
  [glob -nocomplain $core_path/*.vhd] \
  [glob -nocomplain $core_path/*.vhdl]]
set cfg [glob -nocomplain $core_path/core_config.tcl]
set inputs [concat $rtl $cfg]

# Helper: latest mtime in a list (0 if empty)
proc latest_mtime {files} {
  set latest 0
  foreach f $files {
    if {[file exists $f]} {
      set m [file mtime $f]
      if {$m > $latest} { set latest $m }
    }
  }
  return $latest
}

# Early exit if up-to-date (fast path)
if {[file exists $comp_xml]} {
  set in_mtime  [latest_mtime $inputs]
  set out_mtime [file mtime $comp_xml]
  if {$in_mtime != 0 && $in_mtime <= $out_mtime} {
    puts "$core_name up-to-date"
    exit 0
  }
}

# Ensure clean, per-core workspace without deleting other cores’ artifacts
file mkdir $out_dir
set proj_root $output_path/$project_name

# Create a fresh (local) Vivado project for packaging
# (Avoid global deletions; restrict to this core’s temp project)
if {[file exists $proj_root.xpr]} {
  close_project -quiet
  file delete -force $proj_root.cache $proj_root.hw $proj_root.ip_user_files $proj_root.sim $proj_root.xpr
}

create_project -part $part $project_name $output_path -force

# Add sources
foreach f $rtl { add_files -norecurse $f }

# Remove testbenches (fix: use $testbench_files and -nocomplain)
set testbench_files [glob -nocomplain $core_path/*_tb.v $core_path/*_tb.vh $core_path/*_tb.sv $core_path/*_tb.vhd $core_path/*_tb.vhdl]
if {[llength $testbench_files] > 0} {
  remove_files $testbench_files
}

# Add .mem files for BRAM initialization (used by XPM_MEMORY)
set mem_files [glob -nocomplain $core_path/*.mem]
if {[llength $mem_files] > 0} {
  add_files -norecurse $mem_files
  puts "Added [llength $mem_files] .mem file(s) for BRAM initialization"
}

ipx::package_project -import_files -root_dir $out_dir -force

set core [ipx::current_core]

set_property VERSION  $version      $core
set_property NAME     $project_name $core
set_property LIBRARY  {user}        $core
set_property SUPPORTED_FAMILIES {zynq Production zynquplus Production} $core

# Param helper
proc core_parameter {name display_name description} {
  set core [ipx::current_core]
  set p1 [ipx::get_user_parameters $name -of_objects $core]
  if {[llength $p1]} {
    set_property DISPLAY_NAME $display_name $p1
    set_property DESCRIPTION  $description  $p1
  }
  set p2 [ipgui::get_guiparamspec -name $name -component $core]
  if {[llength $p2]} {
    set_property DISPLAY_NAME $display_name $p2
    set_property TOOLTIP      $description  $p2
  }
}

# Core config (defines core_parameter calls)
source $core_path/core_config.tcl
rename core_parameter {}

# Finalize
ipx::create_xgui_files $core
ipx::update_checksums  $core
ipx::save_core         $core

close_project -quiet
puts "$core_name rebuilt"