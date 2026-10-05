# Run after make xpr, passing the analyzer .xpr as the first argument.
open_project [lindex $argv 0]
open_bd_design [get_files */system.bd]
validate_bd_design

proc require_same_net {left right} {
    # Include segments on both sides of a hierarchical boundary (e.g. demod).
    set a [get_bd_nets -boundary_type both -of_objects [get_bd_pins $left]]
    set b [get_bd_nets -boundary_type both -of_objects [get_bd_pins $right]]
    foreach net $a {
        if {[lsearch -exact $b $net] >= 0} {return}
    }
    error "Wrong connection: $left -> $right"
}

set rounding_seeds {0x9e3779b97f4a7c15 0xd1b54a32d192ed03}
for {set channel 0} {$channel < 2} {incr channel} {
    require_same_net awg/dac${channel}_data adc_dac/dac$channel
    require_same_net dds$channel/m_axis_data_tdata cordic$channel/s_axis_data_b
    require_same_net dds$channel/m_axis_data_tvalid cordic$channel/s_axis_tvalid
    if {[get_bd_nets -of_objects [get_bd_pins awg/dac${channel}_data]] eq
        [get_bd_nets -of_objects [get_bd_pins cordic$channel/s_axis_data_b]]} {
        error "Modulated stimulus entered a reference mixer"
    }

    set path cordic$channel
    set lfsr [get_bd_cells $path/lfsr]
    if {[get_property CONFIG.SEED $lfsr] != [lindex $rounding_seeds $channel] ||
        [get_property CONFIG.FEEDBACK_MASK $lfsr] != 0xd800000000000000 ||
        [get_property CONFIG.FEEDBACK_XNOR $lfsr] != 0} {
        error "Wrong phase-extraction rounding sequence on channel $channel"
    }
    require_same_net $path/lfsr/m_axis_tdata $path/complex_mult/s_axis_ctrl_tdata
    for {set component 0} {$component < 2} {incr component} {
        set filter $path/prefilter$component
        if {[get_property VLNV [get_bd_cells $filter]] ne "koheron:user:phase_prefilter:1.0"} {
            error "Missing full-precision prefilter: $filter"
        }
        require_same_net $filter/clk $path/lfsr/aclk
        require_same_net $filter/aresetn $path/lfsr/aresetn
        require_same_net $filter/dout $path/concat_dout_dout/In$component

        set low [expr {16 * $component}]
        set high [expr {$low + 15}]
        set data_slice $path/slice_${high}_${low}_ccomplex_mult_m_axis_dout_tdata
        require_same_net $path/complex_mult/m_axis_dout_tdata $data_slice/Din
        require_same_net $data_slice/Dout $filter/din

        set low [expr {16 * ($component + 1)}]
        set high [expr {$low + 15}]
        set random_slice $path/slice_${high}_${low}_clfsr_m_axis_tdata
        require_same_net $path/lfsr/m_axis_tdata $random_slice/Din
        require_same_net $random_slice/Dout $filter/random_round
    }
    require_same_net $path/concat_dout_dout/dout $path/cordic/s_axis_cartesian_tdata
    require_same_net $path/concat_dout_dout/dout $path/demod
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
source [file join [file dirname [info script]] check_dma.tcl]
puts "PASS: independent DAC stimulus and analyzer reference paths; 8 KiB AXI window"
puts "PASS: four phase prefilters, I/Q ordering, resets and independently seeded rounding"
close_project
