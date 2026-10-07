# Run a second setup pass after the first hold repair has fixed the DAC
# handoff. These physical changes add no registers or feedback latency.
phys_opt_design -hold_fix
route_design -preserve
phys_opt_design -directive AggressiveExplore
source [file normalize [file join [file dirname [info script]] ../../../../fpga/lib/post_route_hold_fix.tcl]]

source [file normalize [file join [file dirname [info script]] reference_launch_routing.tcl]]
if {[dpll_reuse_reference_launch]} {
    route_design -preserve
    phys_opt_design -directive AggressiveExplore
    source [file normalize [file join [file dirname [info script]] ../../../../fpga/lib/post_route_hold_fix.tcl]]
}

# The former vendor-CORDIC netlist's fixed BELs conflict with this extractor's
# placement. Keep automatic physical optimization and the strict board timing
# checks; qualify any new placement against the complete instrument.
