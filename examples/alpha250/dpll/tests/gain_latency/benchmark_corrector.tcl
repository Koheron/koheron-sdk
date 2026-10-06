if {$argc!=2 && $argc!=5 && $argc!=6} {error "Expected fused flag, output directory, optional gain stages / final CSA levels / carry block / optional tail stages"}
set generics [list FUSED=[lindex $argv 0]]
if {$argc>=5} {
    lappend generics GAIN_STAGES=[lindex $argv 2] FINAL_CSA_LEVELS=[lindex $argv 3] CARRY_BLOCK=[lindex $argv 4]
}
if {$argc==6} {lappend generics TAIL_GAIN_STAGES=[lindex $argv 5]}
set fused [lindex $argv 0]
set out [file normalize [lindex $argv 1]]
set here [file dirname [file normalize [info script]]]
file mkdir $out
cd $out
create_project -in_memory -part xc7z020clg400-2
set_param general.maxThreads 4
read_verilog [list $here/../../table_gain.v $here/../../table_corrector.v $here/table_corrector_benchmark.v]
synth_design -top table_corrector_benchmark -mode out_of_context -generic $generics -flatten_hierarchy rebuilt
create_clock -name clk -period 4 [get_ports clk]
set_clock_uncertainty 0.100 [get_clocks clk]
set_false_path -from [get_ports {freq[*] phase[*] enabled[*] active_banks[*] table_command[*] table_data[*]}]
set_false_path -to [get_ports {result[*]}]
opt_design
place_design
phys_opt_design
route_design
source $here/report.tcl
gain_report corrector-fused-$fused $out
write_checkpoint -force $out/routed.dcp
