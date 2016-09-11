
source fpga/scripts/block_design.tcl

generate_target all [get_files $bd_path/system.bd]
make_wrapper -files [get_files $bd_path/system.bd] -top

add_files -norecurse $bd_path/hdl/system_wrapper.v

# Add verilog source files
set files [glob -nocomplain $instrument_path/$instrument_name/*.v $instrument_path/$instrument_name/*.sv]
if {[llength $files] > 0} {
  add_files -norecurse $files
}

# Add constraint files
set fp [open tmp/$instrument_name.xdc r]
set files [split [read $fp]]

close $fp
if {[llength $files] > 0} {
  add_files -norecurse -fileset constrs_1 $files
}

set_property VERILOG_DEFINE {TOOL_VIVADO} [current_fileset]

set_property STRATEGY Flow_PerfOptimized_High [get_runs synth_1]
set_property STRATEGY Performance_NetDelay_high [get_runs impl_1]

close_project
