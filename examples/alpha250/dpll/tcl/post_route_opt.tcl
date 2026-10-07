# Run a second setup pass after the first hold repair has fixed the DAC
# handoff. These physical changes add no registers or feedback latency.
phys_opt_design -hold_fix
route_design -preserve
phys_opt_design -directive AggressiveExplore
source [file normalize [file join [file dirname [info script]] ../../../../fpga/lib/post_route_hold_fix.tcl]]

# Keep these launch registers near their routed consumers. These placements
# close the remaining DAC handoff and carry-input paths at 250 MHz, including
# the board's startup phase allowance. No logical registers are added.
proc dpll_place_launch {name target} {
    set cell [get_cells -quiet $name]
    set bel [get_bels -quiet $target]
    if {[llength $cell] != 1 || [llength $bel] != 1} {
        error "Missing DPLL launch register or placement: $name $target"
    }
    place_cell $cell $target
}
dpll_place_launch {system_i/dac_mux0/inst/dout_reg[0]} SLICE_X110Y130/BFF
dpll_place_launch {system_i/dac_mux0/inst/dout_reg[12]} SLICE_X110Y130/DFF
route_design -preserve
phys_opt_design -directive AggressiveExplore
source [file normalize [file join [file dirname [info script]] ../../../../fpga/lib/post_route_hold_fix.tcl]]

dpll_place_launch {system_i/corrector1/inst/detector/phase_reg[12]_replica} SLICE_X80Y58/D5FF
dpll_place_launch {system_i/corrector0/inst/accurate_controller/gi2/tables.split_final.carry_reg[0]} SLICE_X88Y135/B5FF
route_design -preserve
phys_opt_design -directive AggressiveExplore
source [file normalize [file join [file dirname [info script]] ../../../../fpga/lib/post_route_hold_fix.tcl]]
