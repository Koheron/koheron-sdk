# Run after routing, or source after opening an existing routed checkpoint.
proc gain_report {mode out} {
    report_timing_summary -delay_type min_max -report_unconstrained -file $out/timing.rpt
    report_timing -max_paths 8 -file $out/paths.rpt
    report_utilization -hierarchical -file $out/utilization.rpt
    report_utilization -file $out/total-utilization.rpt
    set path [get_timing_paths -delay_type max -max_paths 1]
    set hold [get_timing_paths -delay_type min -max_paths 1]
    set util [report_utilization -return_string]
    if {![regexp {\|\s*Slice LUTs\*?\s*\|\s*([0-9]+)\s*\|} $util unused lut]} {
        error "Could not extract physical LUT utilization"
    }
    set ff [llength [get_cells -hier -filter {REF_NAME =~ FD*}]]
    set dsp [llength [get_cells -hier -filter {REF_NAME == DSP48E1}]]
    set f [open $out/result.txt w]
    puts $f "mode=$mode wns=[get_property SLACK $path] whs=[get_property SLACK $hold] lut=$lut ff=$ff dsp=$dsp"
    foreach gain {gp gpi gi2 gi3} {
        # Includes input-to-DSP and gain-to-wrapper paths, as well as internal
        # paths. Static gain configuration is timed, with no multicycle cuts.
        set through [get_pins -hier -filter "NAME =~ ${gain}/* || NAME =~ */${gain}/*"]
        if {![llength $through]} {continue}
        set p [get_timing_paths -through $through -max_paths 1]
        if {[llength $p]} {
            puts $f "$gain wns=[get_property SLACK $p]"
            report_timing -through $through -max_paths 1 -file $out/$gain-path.rpt
        }
    }
    close $f
}
