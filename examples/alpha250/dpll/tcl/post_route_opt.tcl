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

# A secondary slice FF can leave a small setup failure on a registered DAC
# handoff even after global optimization. Try a free primary FF in the same
# slice, preserving the register and clock boundary. No device/site is fixed.
proc dpll_reseat_dac_output_registers {} {
    set changed 0
    foreach path [get_timing_paths -delay_type max -slack_lesser_than 0 \
                      -max_paths 16 -nworst 1 -no_report_unconstrained] {
        set source [get_cells -of_objects [get_pins [get_property STARTPOINT_PIN $path]]]
        set destination [get_cells -of_objects [get_pins [get_property ENDPOINT_PIN $path]]]
        if {[llength $source] != 1 || [llength $destination] != 1 ||
            [get_property REF_NAME $source] ne "FDRE" ||
            [get_property REF_NAME $destination] ne "OSERDESE3" ||
            ![string match */dac_mux*/inst/dout_reg* $source] ||
            ![string match *FF2 [get_property BEL $source]]} {continue}
        set site [get_property LOC $source]
        foreach letter {A B C D E F G H} {
            set bel [get_bels -quiet $site/${letter}FF]
            if {![llength $bel] || [llength [get_cells -quiet -of_objects $bel]]} {continue}
            place_cell [list $source $bel]
            puts "DPLL DAC output register placement: $source -> $bel"
            set changed 1
            break
        }
    }
    return $changed
}
if {[dpll_reseat_dac_output_registers]} {
    route_design -preserve
    source [file normalize [file join [file dirname [info script]] ../../../../fpga/lib/post_route_hold_fix.tcl]]
}
