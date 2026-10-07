# ALPHA250's DAC pins are in X1Y2. Keep the existing mux output registers
# nearby so the handoff also meets timing after the startup clock shift.
source [file normalize [file join [file dirname [info script]] gain_programming_timing.tcl]]
set dac_regs [get_cells -hier -filter {REF_NAME == FDRE && NAME =~ *dac_mux*/inst/dout_reg*}]
if {[llength $dac_regs] != 32} {error "Expected 32 DAC mux output registers"}
create_pblock dpll_dac_handoff
add_cells_to_pblock [get_pblocks dpll_dac_handoff] $dac_regs
resize_pblock [get_pblocks dpll_dac_handoff] -add {CLOCKREGION_X1Y2}
set_property IS_SOFT false [get_pblocks dpll_dac_handoff]
