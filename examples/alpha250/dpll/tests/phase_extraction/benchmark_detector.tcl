if {$argc < 2 || $argc > 6} {error "Expected rotations-per-clock, output-directory, optional pair-start, compact-preparation, fused-rounding, residual-correction"}
set rotations [lindex $argv 0]
set pair_start 8
if {$argc >= 3} {set pair_start [lindex $argv 2]}
set compact_prep 0
set fuse_round 1
set residual_correction 1
if {$argc >= 4} {set compact_prep [lindex $argv 3]}
if {$argc >= 5} {set fuse_round [lindex $argv 4]}
if {$argc >= 6} {set residual_correction [lindex $argv 5]}
set groups [expr {$pair_start + (24-$pair_start+$rotations-1)/$rotations}]
set latency [expr {$residual_correction ? 15 : (3-$compact_prep)+$groups+(1-$fuse_round)}]
set out [file normalize [lindex $argv 1]]
set here [file dirname [file normalize [info script]]]
file mkdir $out
cd $out
create_project -in_memory -part xc7z020clg400-2
set_param general.maxThreads 4
read_verilog [list $here/../../phase_residual.v $here/../../phase_extractor.v $here/../../../../../fpga/cores/phase_unwrapper_v1_0/phase_unwrapper.v $here/detector_benchmark.v]
synth_design -top phase_extraction_benchmark -mode out_of_context -generic [list ROTATIONS_PER_CLOCK=$rotations PAIR_START=$pair_start COMPACT_PREP=$compact_prep FUSE_ROUND=$fuse_round RESIDUAL_CORRECTION=$residual_correction] -flatten_hierarchy rebuilt
create_clock -name clk -period 4 [get_ports clk]
set_clock_uncertainty 0.100 [get_clocks clk]
# Wrapper registers define boundaries; every internal path remains timed.
set_false_path -from [get_ports {i_in[*] q_in[*] resetn valid}]
set_false_path -to [get_ports {phase[*] phase_valid frequency[*] error}]
opt_design
place_design
phys_opt_design
route_design
phys_opt_design -directive AggressiveExplore
route_design -preserve
phys_opt_design -hold_fix
route_design -preserve
report_timing_summary -delay_type min_max -report_unconstrained -file $out/timing.rpt
report_timing -max_paths 8 -file $out/paths.rpt
report_utilization -file $out/utilization.rpt
set setup [get_property SLACK [get_timing_paths -delay_type max -max_paths 1]]
set hold [get_property SLACK [get_timing_paths -delay_type min -max_paths 1]]
set f [open $out/result.txt w]
puts $f "rotations_per_clock=$rotations pair_start=$pair_start compact_prep=$compact_prep fuse_round=$fuse_round residual_correction=$residual_correction extraction_latency_clocks=$latency unwrap_latency_clocks=2 combined_latency_clocks=[expr {$latency+2}] setup_slack_ns=$setup hold_slack_ns=$hold"
close $f
write_checkpoint -force $out/routed.dcp
puts "Phase detector route: rotations=$rotations setup=$setup hold=$hold"
if {$setup < 0 || $hold < 0} {error "Phase detector does not meet 250 MHz"}
