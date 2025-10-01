source [file join [file dirname [info script]] "block_design.tcl"]

set_property synth_checkpoint_mode None [get_files $bd_path/system.bd]

generate_target all [get_files $bd_path/system.bd]
make_wrapper -files [get_files $bd_path/system.bd] -top

add_files -norecurse $bd_path/hdl/system_wrapper.v

# Add verilog source files
set files [glob -nocomplain $project_path/*.v $project_path/*.sv]
if {[llength $files] > 0} {
  add_files -norecurse $files
}

set constr_files {}
foreach pat [split $::env(XDC)] {
    lappend constr_files {*}[glob -nocomplain -- $pat]
}
add_files -norecurse -fileset constrs_1 $constr_files

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
