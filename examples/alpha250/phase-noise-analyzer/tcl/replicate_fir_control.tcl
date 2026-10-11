# The FIR FIFO's registered not-full flag reaches thousands of control loads,
# including DSP clock enables. Replicate its launch register after placement
# to shorten those routes without adding a pipeline stage or changing the FIR.
# Select by connectivity rather than the vendor core's private register names.
set fir_sources [get_cells -quiet -hier -filter {REF_NAME == FDRE && NAME =~ system_i/fir/*}]
if {![llength $fir_sources]} {error "Missing FIR control registers"}
set fir_nets [get_nets -quiet -of_objects [get_pins -quiet -of_objects $fir_sources -filter {REF_PIN_NAME == Q}]]
set fir_enable_nets {}
foreach net $fir_nets {
    set sinks [get_pins -quiet -leaf -of_objects $net -filter {DIRECTION == IN}]
    if {[llength $sinks] <= 512} {continue}
    set enable_pins [filter $sinks {REF_PIN_NAME =~ CE*}]
    set dsps [get_cells -quiet -of_objects $enable_pins -filter {REF_NAME == DSP48E1}]
    if {![llength $dsps]} {continue}
    puts "FIR control replication: [llength $sinks] loads, [llength $dsps] DSP enables"
    lappend fir_enable_nets $net
}
if {![llength $fir_enable_nets]} {error "No high-fanout registered FIR DSP enable found"}
# This is a physical-optimization target; Vivado groups loads by placement.
set_property FORCE_MAX_FANOUT 64 $fir_enable_nets
phys_opt_design -force_replication_on_nets $fir_enable_nets
