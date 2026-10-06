# The programmable CIC scaler subtracts one from a 13-bit rate, then uses
# those bits in a lookup table. Its carry outputs have high fanout and cannot
# be replicated directly. Remap only this short chain to equivalent LUTs.
set scaler_carries [get_cells -hier -filter {REF_NAME == CARRY4 && NAME =~ *cic*gen_scaler*}]
if {[llength $scaler_carries] == 0} {error "Expected the programmable CIC rate scaler"}
set_property CARRY_REMAP 4 $scaler_carries
opt_design -property_opt_only

# Allow independent placement of the scaler's lookup muxes as well.
set scaler_muxes [get_cells -hier -filter {REF_NAME =~ MUXF* && NAME =~ *cic*gen_scaler*}]
if {[llength $scaler_muxes] == 0} {error "Expected CIC scaler lookup muxes"}
set_property MUXF_REMAP true $scaler_muxes
opt_design -property_opt_only

# ALPHA250's DAC pins are in X1Y2. Keep the existing mux output registers
# nearby so the handoff also meets timing after the startup clock shift.
set dac_regs [get_cells -hier -filter {REF_NAME == FDRE && NAME =~ *dac_mux*/inst/dout_reg*}]
if {[llength $dac_regs] != 32} {error "Expected 32 DAC mux output registers"}
create_pblock dpll_dac_handoff
add_cells_to_pblock [get_pblocks dpll_dac_handoff] $dac_regs
resize_pblock [get_pblocks dpll_dac_handoff] -add {CLOCKREGION_X1Y2}
set_property IS_SOFT false [get_pblocks dpll_dac_handoff]
