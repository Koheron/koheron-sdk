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
    if {[get_property CONFIG.LOOKAHEAD_HISTORY [get_bd_cells cordic$channel/phase_unwrapper]] != 1} {
        error "Expected cached carry prefixes for 250 MHz history"
    }
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
pna_same_net ps_0/FCLK_CLK0 adc/ps_clk
pna_same_net adc/ps_clk adc/mmcm/psclk
pna_same_net ps_ctl/mmcm_ps adc/ctl
pna_same_net ps_0/FCLK_CLK0 ps_ctl/aclk
pna_same_net ps_ctl/aclk ps_ctl/axi_ps_ctl_register/aclk
puts "PASS: MMCM reset and phase commands use the independent PS clock/control path"
close_project
