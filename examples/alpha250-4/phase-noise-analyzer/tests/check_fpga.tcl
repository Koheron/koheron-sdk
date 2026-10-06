# Run after make xpr, passing the analyzer .xpr as the first argument.
open_project [lindex $argv 0]
open_bd_design [get_files */system.bd]
validate_bd_design
if {[get_property CONFIG.PREDECODE_WRITES [get_bd_cells ctl/axi_ctl_register]] != 1} {
    error "Expected predecoded AXI settings writes at 250 MHz"
}
source [file join [file dirname [info script]] ../../../alpha250/phase-noise-analyzer/tests/check_extractor.tcl]
for {set channel 0} {$channel < 4} {incr channel} {
    check_phase_extractor cordic$channel
    foreach {key expected} {Multiplier_Construction Use_Mults PipeStages 8 PortAWidth 64 PortBWidth 32 OutputWidthHigh 93 OutputWidthLow 30} {
        if {[get_property CONFIG.$key [get_bd_cells scaler$channel]] ne $expected} {
            error "scaler$channel: unexpected $key"
        }
    }
    pna_same_net dds$channel/m_axis_data_tdata cordic$channel/s_axis_data_b
    pna_same_net dds$channel/m_axis_data_tvalid cordic$channel/s_axis_tvalid
}
puts "PASS: four shared 24-bit Cartesian/phase extractors and legacy demod scaling"
for {set stream 0} {$stream < 2} {incr stream} {
    pna_same_net adc/adc_clk phase_fixed_decimator$stream/aclk
    pna_same_net adc/adc_clk phase_cic_clock_converter$stream/s_axis_aclk
    pna_same_net ps_0/FCLK_CLK1 phase_cic_clock_converter$stream/m_axis_aclk
    foreach core {cic fir phase_quantizer} {
        pna_same_net ps_0/FCLK_CLK1 $core$stream/aclk
        pna_same_net phase_filter_cdc/resetn $core$stream/aresetn
    }
    pna_same_net paired_cic_control/data_valid phase_fixed_decimator$stream/s_axis_tvalid
    pna_same_net paired_cic_control/data_ready_[lindex {x y} $stream] phase_fixed_decimator$stream/s_axis_tready
    pna_same_net phase_filter_cdc/rate cic$stream/total_rate
}
puts "PASS: paired fixed /2 and synchronized X/Y admission, slow CIC/FIR/packets"
close_project
