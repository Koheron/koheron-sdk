# Table write addresses fan out to thousands of distributed RAM bits. Force
# physical copies of their existing launch registers after placement, even
# when the estimated slack is positive. This adds no programming or loop delay.
set sources [get_cells -hier -filter {REF_NAME == FDRE && NAME =~ *gain_programmer/inst/command*_reg*}]
if {[llength $sources] != 18} {error "Expected both nine-bit gain programming commands"}
set nets [get_nets -of_objects [get_pins -of_objects $sources -filter {REF_PIN_NAME == Q}]]
phys_opt_design -force_replication_on_nets $nets
