# A physical projection copy can land beyond the captured-origin registers,
# whose different clock enables prevent moving it beside the reference carry.
# Reuse the existing projection launch on the other side of that carry only
# when its data, clock, controls and initialization are identical.
proc dpll_reuse_reference_launch {} {
    set nearby [get_cells -quiet {system_i/corrector0/inst/detector/phase_reg[9]}]
    set distant [get_cells -quiet {system_i/corrector0/inst/detector/phase_reg[9]_replica_1}]
    if {[llength $nearby]!=1 || [llength $distant]!=1} {return false}
    set paths [get_timing_paths -from $distant -max_paths 1 -no_report_unconstrained]
    if {![llength $paths] || [get_property SLACK $paths]>=0} {return false}
    if {[get_property REF_NAME $nearby] ne [get_property REF_NAME $distant] ||
        [get_property INIT $nearby] ne [get_property INIT $distant]} {
        error "Different projection registers in reference routing"
    }
    foreach role {D C CE R} {
        set a [get_nets -of_objects [get_pins -of_objects $nearby -filter "REF_PIN_NAME == $role"]]
        set b [get_nets -of_objects [get_pins -of_objects $distant -filter "REF_PIN_NAME == $role"]]
        if {$a ne $b} {error "Different projection register $role inputs"}
    }
    set nearby_net [get_nets -of_objects [get_pins -of_objects $nearby -filter {REF_PIN_NAME == Q}]]
    set distant_net [get_nets -of_objects [get_pins -of_objects $distant -filter {REF_PIN_NAME == Q}]]
    set loads [get_pins -leaf -of_objects $distant_net -filter {DIRECTION == IN}]
    if {[llength $loads]!=2} {error "Unexpected reference launch consumers"}
    foreach pin $loads {
        if {![string match "system_i/corrector0/inst/capture/*" $pin]} {
            error "Unexpected reference launch pin $pin"
        }
    }
    # Disconnect leaf aliases before connecting across the module boundary.
    disconnect_net -pinlist $loads
    connect_net -hierarchical -basename dpll_reference_launch_near -net $nearby_net -objects $loads
    puts "DPLL reference routing: reused identical projection register for [llength $loads] carry inputs"
    return true
}
