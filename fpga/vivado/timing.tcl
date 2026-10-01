set xpr_filename [lindex $argv 0]
set bit_filename [lindex $argv 1]

open_project $xpr_filename
set impl_run [get_runs impl_1]
open_run $impl_run

source [file join [file dirname [info script]] timing_check.tcl]
koheron_check_routed_timing $impl_run $bit_filename

close_project
