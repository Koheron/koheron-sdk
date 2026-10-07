# Table programming and applied-bank signals fan out to distributed RAM bits. Force
# physical copies of their existing launch registers after placement, even
# when the estimated slack is positive. This adds no programming or loop delay.
set sources [get_cells -hier -filter {REF_NAME == FDRE && NAME =~ *gain_programmer/inst/command*_reg*}]
if {[llength $sources] != 18} {error "Expected both nine-bit gain programming commands"}
set data_sources [get_cells -hier -filter {REF_NAME == FDRE && NAME =~ *gain_programmer/inst/data_reg*}]
# Tables consume 48 bits (Q*.11, 32-bit gain, four-bit nibble). Coefficient
# readback retains all 64 bits directly from the coherent response registers;
# it no longer shares the table-write launch registers.
if {[llength $data_sources] < 48} {error "Expected all 48 table programming data bits"}
set bank_sources [get_cells -hier -filter {REF_NAME == FDRE && NAME =~ *gain_programmer/inst/active_banks_reg*}]
if {[llength $bank_sources] < 8} {error "Expected eight applied gain banks"}
set sources [concat $sources $data_sources $bank_sources]
set nets [get_nets -of_objects [get_pins -of_objects $sources -filter {REF_PIN_NAME == Q}]]
set_property FORCE_MAX_FANOUT 64 $nets
phys_opt_design -force_replication_on_nets $nets

# PI/I2 read addresses fan out across every bit of a nibble's product.
# Replicate their existing launch registers near the LUT RAM, retaining the
# same state, samples and clock boundaries in both feedback controllers.
set address_sources {}
foreach channel {0 1} {
    set selected [get_cells -hier -filter "REF_NAME == FDRE && (NAME =~ system_i/cordic$channel/phase_unwrapper/inst/phase_out_reg* || NAME =~ system_i/corrector$channel/inst/acc1_reg*)"]
    if {[llength $selected] < 32} {error "Missing gain-table read address registers in loop $channel"}
    set address_sources [concat $address_sources $selected]
}
set address_nets [get_nets -of_objects [get_pins -of_objects $address_sources -filter {REF_PIN_NAME == Q}]]
set_property FORCE_MAX_FANOUT 32 $address_nets
phys_opt_design -force_replication_on_nets $address_nets
