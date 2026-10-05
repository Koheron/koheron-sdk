# Shared PNA post-route hook. Keep bundled DAC settings transfers within the
# AWG's existing bus-skew limit as the larger phase extractors change placement.
set pna_hold_hook [file join [file dirname [info script]] post_route_hold_fix.tcl]
source $pna_hold_hook

proc koheron_pna_mailbox_skew_failed {} {
    set report [report_bus_skew -no_detailed_paths -sort_by_slack -return_string]
    set entry ""
    foreach line [split $report "\n"] {
        if {[regexp {^[0-9]+[[:space:]]+[0-9]+[[:space:]]+} $line]} {set entry ""}
        append entry $line "\n"
        if {[regexp {^[[:space:]]*(Slow|Fast)[[:space:]]+\S+[[:space:]]+\S+[[:space:]]+(\S+)[[:space:]]*$} $line -> corner slack] &&
            [string is double -strict $slack] && $slack < 0 &&
            [string match {*controller/mailbox_reg*} $entry]} {return 1}
    }
    return 0
}

# ALPHA250-4 has no DAC; successful routes on any board need no extra pass.
# Limit retries, and retain the SDK's final setup/hold/bus-skew gate. A failed
# route must still fail the build, never relax the CDC constraints.
for {set attempt 0} {$attempt < 2 && [koheron_pna_mailbox_skew_failed]} {incr attempt} {
    set sources [get_cells -quiet -hier -filter {IS_SEQUENTIAL && NAME =~ *controller/mailbox_reg*}]
    set nets [get_nets -quiet -of_objects [get_pins -quiet -of_objects $sources -filter {REF_PIN_NAME == Q}]]
    if {[llength $nets] == 0} {break}
    puts "PNA: rerouting [llength $nets] DAC mailbox nets to meet bus skew (attempt [expr {$attempt+1}])"
    route_design -unroute -nets $nets
    route_design -directive Explore
    source $pna_hold_hook
}
