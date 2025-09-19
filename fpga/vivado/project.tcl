source [file join [file dirname [info script]] "block_design.tcl"]

if {[version -short] >= 2016.3} {
  set_property synth_checkpoint_mode None [get_files $bd_path/system.bd]
}

generate_target all [get_files $bd_path/system.bd]
make_wrapper -files [get_files $bd_path/system.bd] -top

add_files -norecurse $bd_path/hdl/system_wrapper.v

# Add verilog source files
set files [glob -nocomplain $project_path/*.v $project_path/*.sv]
if {[llength $files] > 0} {
  add_files -norecurse $files
}

# Add constraint files
set constr_files {}
if {[info exists xdc_filename]} {
  if {[file isfile $xdc_filename]} {
    set fp [open $xdc_filename r]
    set content [read $fp]
    close $fp
    foreach f $content { if {$f ne ""} { lappend constr_files $f } }
  } elseif {[file isdirectory $xdc_filename]} {
    set constr_files [glob -nocomplain -directory $xdc_filename *.xdc]
  } else {
    puts "WARNING: constraints path '$xdc_filename' does not exist"
  }
} else {
  puts "WARNING: xdc_filename not set; no constraints will be added"
}
if {[llength $constr_files] > 0} {
  add_files -norecurse -fileset constrs_1 $constr_files
}


set_property VERILOG_DEFINE {TOOL_VIVADO} [current_fileset]

switch $mode {
  "development" {
    # set_property STRATEGY Flow_PerfOptimized_High [get_runs synth_1]
    # set_property STRATEGY Performance_NetDelay_high [get_runs impl_1]
  }
  "production" {
    set_property STRATEGY Flow_PerfOptimized_High [get_runs synth_1]
    set_property STRATEGY Performance_NetDelay_high [get_runs impl_1]
  }
  "custom" {
    # Put your custom implementation strategy here (and run $ make MODE=custom ...)
  }
  default {
  }
}

close_project
