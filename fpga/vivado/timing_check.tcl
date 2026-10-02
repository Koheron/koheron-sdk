# Vivado reports NA for bus skew when optimization leaves only one data bit.
# Accept that case only after checking the routed connectivity and its timing.
proc koheron_single_bit_skew {entry} {
  if {![regexp {^[0-9]+[[:space:]]+[0-9]+[[:space:]]+\[get_cells \{([^{}]+)\}\]} $entry -> source]} {
    return 0
  }
  set cells [regexp -all -inline {\{([^{}]+)\}} $entry]
  set source_cell [get_cells -quiet $source]
  if {[llength $source_cell] != 1} { return 0 }
  set active {}
  foreach {match name} [lrange $cells 2 end] {
    set cell [get_cells -quiet $name]
    if {[llength $cell] != 1} { return 0 }
    set data [get_pins -quiet -of_objects $cell -filter {REF_PIN_NAME == D}]
    if {[llength $data] != 1} { return 0 }
    set drivers [get_pins -quiet -of_objects [get_nets -of_objects $data] -filter {DIRECTION == OUT}]
    if {[llength $drivers] != 1} { return 0 }
    set driver_cell [get_cells -of_objects $drivers]
    if {[get_property REF_NAME $driver_cell] in {GND VCC}} { continue }
    if {$driver_cell ne $source_cell} { return 0 }
    lappend active $cell
  }
  if {[llength $active] != 1} { return 0 }
  set paths [get_timing_paths -from $source_cell -to $active -max_paths 1 -no_report_unconstrained]
  if {[llength $paths] != 1} { return 0 }
  set slack [get_property SLACK $paths]
  return [expr {[string is double -strict $slack] && $slack >= 0}]
}

proc koheron_check_routed_timing {run bit_filename} {
  set report_file "[file rootname $bit_filename]_timing_summary.rpt"
  set bus_skew_file "[file rootname $bit_filename]_bus_skew.rpt"
  report_timing_summary -report_unconstrained -file $report_file
  report_bus_skew -no_detailed_paths -sort_by_slack -file $bus_skew_file

  set file [open $report_file r]
  set timing_report [read $file]
  close $file
  if {[regexp {checking no_clock \(([0-9]+)\)} $timing_report -> unclocked] && $unclocked > 0} {
    error "Routed design has $unclocked clock pins without a timing clock. See $report_file"
  }
  if {[regexp {checking no_input_delay \(([0-9]+)\)} $timing_report -> missing_inputs] &&
      [regexp {checking no_output_delay \(([0-9]+)\)} $timing_report -> missing_outputs] &&
      ($missing_inputs > 0 || $missing_outputs > 0)} {
    puts "WARNING: I/O timing is incomplete: $missing_inputs inputs and $missing_outputs outputs lack delay constraints. See $report_file"
  }

  set violations {}
  foreach kind {setup hold} {
    set paths [get_timing_paths -$kind -max_paths 1 -no_report_unconstrained]
    if {$kind eq "setup" && [llength $paths] == 0} {
      error "No constrained setup paths found. See $report_file"
    }
    foreach path $paths {
      set slack [get_property SLACK $path]
      if {![string is double -strict $slack]} {
        error "Routed $kind slack is unavailable. See $report_file"
      }
      if {$slack < 0} {
        lappend violations "$kind slack=$slack ns"
      }
    }
  }

  foreach metric {WNS TNS WHS THS TPWS} {
    set value [get_property STATS.$metric $run]
    if {![string is double -strict $value]} {
      error "Routed timing metric $metric is unavailable. See $report_file"
    }
    if {$value < 0} {
      lappend violations "$metric=$value ns"
    }
    set timing($metric) $value
  }

  set file [open $bus_skew_file r]
  set bus_skew_report [read $file]
  close $file
  set constraints 0
  set slacks 0
  set single_bits 0
  set entry ""
  if {![string match {*No bus skew constraints*} $bus_skew_report]} {
    if {![string match {*Slack(ns)*} $bus_skew_report]} {
      error "Bus skew report has no summary table. See $bus_skew_file"
    }
    foreach line [split $bus_skew_report "\n"] {
      if {[regexp {^[0-9]+[[:space:]]+[0-9]+[[:space:]]+} $line]} {
        incr constraints
        set entry ""
      }
      append entry $line "\n"
      if {[regexp {^[[:space:]]*(Slow|Fast|NA)[[:space:]]+\S+[[:space:]]+\S+[[:space:]]+(\S+)[[:space:]]*$} $line -> corner slack]} {
        if {$corner eq "NA" || ![string is double -strict $slack]} {
          if {$corner eq "NA" && [koheron_single_bit_skew $entry]} {
            incr single_bits
            incr slacks
            continue
          }
          error "Bus skew could not be analyzed; check clock constraints. See $bus_skew_file"
        }
        incr slacks
        if {$slack < 0} {
          lappend violations "bus skew slack=$slack ns"
        }
      }
    }
  }
  if {$slacks != $constraints || ($constraints == 0 && ![string match {*No bus skew constraints*} $bus_skew_report])} {
    error "Could not read all bus skew constraints ($slacks/$constraints). See $bus_skew_file"
  }

  if {[llength $violations] > 0} {
    error "Routed timing failed: [join $violations {, }]. See $report_file and $bus_skew_file"
  }
  puts "Routed timing met: WNS=$timing(WNS) ns, WHS=$timing(WHS) ns, TPWS=$timing(TPWS) ns; $constraints bus skew constraints checked ($single_bits reduced to one timed bit). Reports: $report_file, $bus_skew_file"
}
