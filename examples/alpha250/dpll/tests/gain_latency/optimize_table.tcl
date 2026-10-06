if {$argc!=2} {error "Expected input checkpoint and output directory"}
set checkpoint [file normalize [lindex $argv 0]]
set out [file normalize [lindex $argv 1]]
file mkdir $out
open_checkpoint $checkpoint
set_param general.maxThreads 4
phys_opt_design -directive AggressiveFanoutOpt
route_design -directive AggressiveExplore
source [file join [file dirname [info script]] report.tcl]
gain_report optimized $out
write_checkpoint -force $out/routed.dcp
