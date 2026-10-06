if {$argc != 1} { error "Expected output-directory containing mode-* checkpoints" }
set root [file normalize [lindex $argv 0]]
source [file join [file dirname [info script]] report.tcl]
foreach dcp [glob $root/mode-*/routed.dcp] {
    set out [file dirname $dcp]
    regexp {mode-([0-9]+)$} $out unused mode
    open_checkpoint $dcp
    gain_report $mode $out
    close_design
}
