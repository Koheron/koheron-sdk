# Run after make xpr, passing the analyzer .xpr as the first argument.
open_project [lindex $argv 0]
open_bd_design [get_files */system.bd]
validate_bd_design
source [file join [file dirname [info script]] ../../../alpha250/phase-noise-analyzer/tests/check_extractor.tcl]
for {set channel 0} {$channel < 4} {incr channel} {
    check_phase_extractor cordic$channel
    pna_same_net dds$channel/m_axis_data_tdata cordic$channel/s_axis_data_b
    pna_same_net dds$channel/m_axis_data_tvalid cordic$channel/s_axis_tvalid
}
puts "PASS: four shared 24-bit Cartesian/phase extractors and legacy demod scaling"
close_project
