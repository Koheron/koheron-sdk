# Table programming and applied-bank signals fan out to distributed RAM bits. Force
# physical copies of their existing launch registers after placement, even
# when the estimated slack is positive. This adds no programming or loop delay.
set sources [get_cells -hier -filter {REF_NAME == FDRE && NAME =~ *gain_programmer/inst/command*_reg*}]
if {[llength $sources] != 18} {error "Expected both nine-bit gain programming commands"}
set data_sources [get_cells -hier -filter {REF_NAME == FDRE && NAME =~ *gain_programmer/inst/data_reg*}]
if {[llength $data_sources] < 64} {error "Expected the 64-bit gain programming payload"}
set bank_sources [get_cells -hier -filter {REF_NAME == FDRE && NAME =~ *gain_programmer/inst/active_banks_reg*}]
if {[llength $bank_sources] < 8} {error "Expected eight applied gain banks"}
set sources [concat $sources $data_sources $bank_sources]
set nets [get_nets -of_objects [get_pins -of_objects $sources -filter {REF_PIN_NAME == Q}]]
set_property FORCE_MAX_FANOUT 64 $nets
phys_opt_design -force_replication_on_nets $nets
