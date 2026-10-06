if {$argc != 2} { error "Expected mode and output-directory" }
set mode [lindex $argv 0]
set out [file normalize [lindex $argv 1]]
set here [file dirname [file normalize [info script]]]
file mkdir $out
cd $out
create_project -in_memory -part xc7z020clg400-2
set_param general.maxThreads 4
read_verilog [list $here/../../gain_multiplier.v $here/geometric_gain.v $here/constant_gain.v $here/../../table_gain.v $here/benchmark.v]
synth_design -top gain_benchmark -mode out_of_context -generic MODE=$mode -flatten_hierarchy none
create_clock -name clk -period 4.000 [get_ports clk]
set_clock_uncertainty 0.100 [get_clocks clk]
# This is a registered-boundary, out-of-context experiment, not package I/O
# timing. All measured data paths are between the wrapper/gain registers.
set_false_path -from [get_ports {samples[*] coefficients[*] octaves[*] table_command[*] table_data[*] active_banks[*]}]
set_false_path -to [get_ports {result[*]}]
opt_design
place_design
phys_opt_design
route_design
write_checkpoint -force $out/routed.dcp
source $here/report.tcl
gain_report $mode $out
close_project
