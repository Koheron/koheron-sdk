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
  if {![string match {*No bus skew constraints*} $bus_skew_report]} {
    if {![string match {*Slack(ns)*} $bus_skew_report]} {
      error "Bus skew report has no summary table. See $bus_skew_file"
    }
    foreach line [split $bus_skew_report "\n"] {
      if {[regexp {^[0-9]+[[:space:]]+[0-9]+[[:space:]]+} $line]} {
        incr constraints
      }
      if {[regexp {^[[:space:]]*(Slow|Fast|NA)[[:space:]]+\S+[[:space:]]+\S+[[:space:]]+(\S+)[[:space:]]*$} $line -> corner slack]} {
        if {$corner eq "NA" || ![string is double -strict $slack]} {
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
  puts "Routed timing met: WNS=$timing(WNS) ns, WHS=$timing(WHS) ns, TPWS=$timing(TPWS) ns; $constraints bus skew constraints met. Reports: $report_file, $bus_skew_file"
}
