# Run after make xpr, passing the analyzer .xpr as the first argument.
open_project [lindex $argv 0]
open_bd_design [get_files */system.bd]
validate_bd_design

proc require_same_net {left right} {
    set a [get_bd_nets -of_objects [get_bd_pins $left]]
    set b [get_bd_nets -of_objects [get_bd_pins $right]]
    if {[llength $a] != 1 || $a ne $b} {error "Wrong connection: $left -> $right"}
}

for {set channel 0} {$channel < 2} {incr channel} {
    require_same_net awg/dac${channel}_data adc_dac/dac$channel
    require_same_net dds$channel/m_axis_data_tdata cordic$channel/s_axis_data_b
    require_same_net dds$channel/m_axis_data_tvalid cordic$channel/s_axis_tvalid
    if {[get_bd_nets -of_objects [get_bd_pins awg/dac${channel}_data]] eq
        [get_bd_nets -of_objects [get_bd_pins cordic$channel/s_axis_data_b]]} {
        error "Modulated stimulus entered a reference mixer"
    }
}
require_same_net awg/sample_clk adc_dac/adc_clk
if {[get_property CONFIG.CHANNELS [get_bd_cells awg]] != 2} {error "Two DAC channels required"}
if {[get_property CONFIG.PHASE_WIDTH [get_bd_cells awg]] != 48} {error "48-bit carrier precision required"}
set segments [get_bd_addr_segs -of_objects [get_bd_addr_spaces ps_0/Data] -filter {NAME =~ *awg*}]
if {[llength $segments] != 1} {error "Missing or duplicate DDS PM address segment"}
set segment [lindex $segments 0]
set bytes [expr [string map {K *1024 M *1024*1024} [get_property RANGE $segment]]]
if {[get_property OFFSET $segment] != 0x44000000 || $bytes != 8192} {
    error "Wrong DDS PM address window"
}
puts "PASS: independent DAC stimulus and analyzer reference paths; 8 KiB AXI window"
close_project
