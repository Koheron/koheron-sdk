# Analyze a routed ALPHA250 checkpoint at the bitstream phase and after the
# normal 250 MS/s startup shift. This changes only the in-memory timing model;
# it does not write a checkpoint/bitstream or program hardware.
# vivado -mode batch -source boards/alpha250/tests/check_dac_timing.tcl \
#   -tclargs routed.dcp reports-directory
if {$argc != 2} {error "Expected routed.dcp and reports-directory"}
open_checkpoint [lindex $argv 0]
set output [lindex $argv 1]
file mkdir $output
set mmcm [get_cells -hier -filter {REF_NAME == MMCME2_ADV}]
if {[llength $mmcm] != 1} {error "Expected one ALPHA250 MMCM"}
if {![get_property CLKOUT0_USE_FINE_PS $mmcm] ||
    [get_property CLKOUT1_USE_FINE_PS $mmcm] ||
    [get_property CLKFBOUT_USE_FINE_PS $mmcm]} {
    error "Unexpected fine-phase configuration; cannot model a CLKOUT0-only shift"
}
set output_pin [get_pins $mmcm/CLKOUT0]
set input_clocks [get_clocks -of_objects [get_pins $mmcm/CLKIN1]]
if {[llength $input_clocks] != 1 ||
    abs([get_property PERIOD $input_clocks] - 4.0) > 0.001} {
    error "This startup-phase check is for the 250 MS/s configuration"
}
set initial_phase [get_property CLKOUT0_PHASE $mmcm]
set divide [get_property CLKOUT0_DIVIDE_F $mmcm]
set failed false
foreach steps {0 56} {
    # UG472: each dynamic increment is 1/56 of the MMCM VCO period.
    # Editing the primitive phase makes Vivado rederive the clock waveform
    # and keeps the complete clock paths for skew and pessimism analysis.
    set phase [expr {$initial_phase + 360.0 * $steps / (56.0 * $divide)}]
    set_property CLKOUT0_PHASE $phase $mmcm
    set src [get_clocks -include_generated_clocks -of_objects $output_pin]
    set dst [get_clocks -include_generated_clocks -of_objects [get_pins $mmcm/CLKOUT1]]
    # The board XDC reserves the runtime shift as setup uncertainty. Here the
    # actual phase is modeled explicitly, so remove that reserve to avoid
    # counting it twice. Vivado's computed jitter/phase error remains active.
    set_clock_uncertainty -setup 0.000 -from $src -to $dst
    foreach {kind label} {max setup min hold} {
        set paths [get_timing_paths -from $src -to $dst -delay_type $kind -slack_lesser_than 1000000 -max_paths 1]
        if {[llength $paths] == 0} {
            error "No DAC $label paths at phase $steps; check timing exceptions"
        }
        set slack [get_property SLACK $paths]
        if {![string is double -strict $slack]} {
            error "DAC $label path has no finite slack at phase $steps"
        }
        puts "DAC_TIMING steps=$steps degrees=$phase check=$label slack_ns=$slack"
        report_timing -from $src -to $dst -delay_type $kind -slack_lesser_than 1000000 -max_paths 64 \
            -file [file join $output phase-${steps}-${label}.rpt]
        if {$slack < 0} {set failed true}
    }
}
if {$failed} {error "DAC setup/hold timing failed; see reports"}
puts "DAC setup and hold pass at phases 0 and 56"
exit
