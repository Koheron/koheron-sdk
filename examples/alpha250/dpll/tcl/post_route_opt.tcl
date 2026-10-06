# Run a second setup pass after the first hold repair has fixed the DAC
# handoff. These physical changes add no registers or feedback latency.
phys_opt_design -hold_fix
route_design -preserve
phys_opt_design -directive AggressiveExplore
source [file normalize [file join [file dirname [info script]] ../../../../fpga/lib/post_route_hold_fix.tcl]]

# The two-loop table implementation leaves DAC0 bit 1 too far from its output
# register after automatic placement. Move the existing mux register toward
# that I/O within dpll_dac_handoff; no register or timing exception is added.
set_param general.maxThreads 4
set dac_handoff [get_cells {system_i/dac_mux0/inst/dout_reg[1]}]
if {[llength $dac_handoff] != 1} {error "Missing DAC0 bit 1 handoff register"}
place_cell $dac_handoff SLICE_X103Y119/BFF
route_design -preserve
phys_opt_design -directive AggressiveExplore
source [file normalize [file join [file dirname [info script]] ../../../../fpga/lib/post_route_hold_fix.tcl]]
