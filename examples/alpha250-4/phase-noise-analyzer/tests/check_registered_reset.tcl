# Require a register source for the reset entering the asynchronous CDC.
# Optional source overrides allow checking a legacy regression fixture.
set repo [file normalize [file join [file dirname [info script]] ../../../..]]
set single [lindex $argv 0]
set paired [lindex $argv 1]
if {$single eq ""} {set single $repo/fpga/cores/phase_stream_control_v1_0/phase_stream_control.v}
if {$paired eq ""} {set paired $repo/examples/alpha250-4/phase-noise-analyzer/paired_cic_control_v1_0/paired_cic_control.v}
foreach spec [list [list phase_stream_control $single 1] [list phase_stream_control $single 2] [list paired_cic_control $paired 0]] {
    lassign $spec top source rate_step
    create_project -in_memory -part xc7z010clg400-1
    read_verilog $source
    if {$rate_step} {
        synth_design -top $top -mode out_of_context -generic RATE_STEP=$rate_step
    } else {
        synth_design -top $top -mode out_of_context
    }
    set output [get_ports filter_resetn]
    set drivers [get_pins -leaf -of_objects [get_nets -segments -of_objects $output] -filter {DIRECTION == OUT}]
    if {[llength $drivers] != 1} {error "$top: reset must have one register driver"}
    set cell [get_cells -of_objects $drivers]
    set kind [get_property REF_NAME $cell]
    if {$kind ni {FDRE FDSE FDCE FDPE} || [get_property REF_PIN_NAME $drivers] ne "Q"} {
        error "$top RATE_STEP=$rate_step: filter_resetn driven by $kind instead of a register Q"
    }
    set clock [get_pins -of_objects $cell -filter {REF_PIN_NAME == C}]
    set sources [all_fanin -flat -startpoints_only -to $clock]
    if {[llength $sources] != 1 || [get_property NAME $sources] ne "aclk"} {
        error "$top: reset register must use the ADC clock"
    }
    puts "PASS: $top RATE_STEP=$rate_step reset driven directly by $kind Q on aclk"
    close_project
}
