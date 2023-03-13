set run_autowrapper 1
source [file join [file dirname [info script]] "block_design.tcl"]
set_property target_language VHDL [current_project]

if {[version -short] >= 2016.3} {
  set_property synth_checkpoint_mode Hierarchical [get_files $bd_path/system.bd]
}

generate_target all [get_files $bd_path/system.bd]
if { $run_autowrapper == 1 } {
  make_wrapper -files [get_files $bd_path/system.bd] -top
  add_files -norecurse $bd_path/hdl/system_wrapper.vhd
}


# Add verilog source files
set files [glob -nocomplain $project_path/*.v $project_path/*.sv]
if {[llength $files] > 0} {
  add_files -norecurse $files
}

# Add constraint files
set fp [open $xdc_filename r]
set files [split [read $fp]]

close $fp
if {[llength $files] > 0} {
  add_files -norecurse -fileset constrs_1 $files
}

set_property VERILOG_DEFINE {TOOL_VIVADO} [current_fileset]

switch $mode {
  "development" {
    set_property STRATEGY Flow_PerfOptimized_High [get_runs synth_1]
    set_property STRATEGY Performance_NetDelay_high [get_runs impl_1]
    generate_target all [get_files  $bd_path/system.bd]
    set_property STEPS.SYNTH_DESIGN.ARGS.FLATTEN_HIERARCHY none [get_runs *synth_1]
    set_property strategy Flow_PerfOptimized_high [get_runs *synth_1]
    set_property STEPS.SYNTH_DESIGN.ARGS.FLATTEN_HIERARCHY none [get_runs *synth_1]
  }
  "production" {
    set_property STRATEGY Flow_PerfOptimized_High [get_runs synth_1]
    set_property STRATEGY Performance_NetDelay_high [get_runs impl_1]
    generate_target all [get_files  $bd_path/system.bd]
    set_property STEPS.SYNTH_DESIGN.ARGS.FLATTEN_HIERARCHY none [get_runs *synth_1]
    set_property strategy Flow_PerfOptimized_high [get_runs *synth_1]
    set_property STEPS.SYNTH_DESIGN.ARGS.FLATTEN_HIERARCHY none [get_runs *synth_1]
  }
  "custom" {
    # Put your custom implementation strategy here (and run $ make MODE=custom ...)    
  }  
  default {
  }
}

close_project
