# Run a second setup pass after the first hold repair has fixed the DAC
# handoff. These physical changes add no registers or feedback latency.
phys_opt_design -hold_fix
route_design -preserve
phys_opt_design -directive AggressiveExplore
source [file normalize [file join [file dirname [info script]] ../../../../fpga/lib/post_route_hold_fix.tcl]]
