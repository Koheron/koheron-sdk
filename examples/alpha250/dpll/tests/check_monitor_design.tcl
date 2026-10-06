if {$argc != 1} {error "Expected the full DPLL project path"}
open_project -read_only [file normalize [lindex $argv 0]]
open_bd_design [get_files */system.bd]
foreach {cell property expected} {
    monitor_cordic/complex_mult APortWidth 16
    monitor_cordic/complex_mult OutputWidth 24
    monitor_cordic/prefilter0 WIDTH 24
    monitor_cordic/prefilter1 WIDTH 24
    monitor_cordic/cordic Input_Width 24
    monitor_cordic/cordic Output_Width 24
    monitor_cordic/cordic Pipelining_Mode Maximum
    monitor_cordic/phase_unwrapper DOUT_WIDTH 64
    monitor_cordic/phase_unwrapper PIPELINED_OVERFLOW 1
    fir Data_Width 40
    fir Output_Width 40
    fir Clock_Frequency 143
    axi_dma_0 c_include_sg 1
} {
    set actual [get_property CONFIG.$property [get_bd_cells $cell]]
    if {[string is double -strict $expected]} {
        if {$actual != $expected} {error "Unexpected monitor setting: $cell $property=$actual"}
    } elseif {$actual ne $expected} {error "Unexpected monitor setting: $cell $property=$actual"}
}
proc same_net {a b} {
    if {[get_bd_nets -of_objects [get_bd_pins $a]] ne [get_bd_nets -of_objects [get_bd_pins $b]]} {
        error "Expected a shared net: $a and $b"
    }
}
same_net fir/aclk ps_0/FCLK_CLK1
same_net phase_quantizer/aclk ps_0/FCLK_CLK1
same_net cic/aclk ps_0/FCLK_CLK1
same_net phase_fixed_decimator/aclk adc_dac/adc_clk
same_net phase_cic_clock_converter/s_axis_aclk adc_dac/adc_clk
same_net phase_cic_clock_converter/m_axis_aclk ps_0/FCLK_CLK1
if {[get_property VLNV [get_bd_cells cic]] ne "koheron:user:phase_cic_decimator:1.0"} {error "Expected shared slow CIC"}
if {[get_property CONFIG.RATE_STEP [get_bd_cells phase_stream_control]] != 2} {error "Expected even rates"}
same_net monitor_cordic/aclk adc_dac/adc_clk
same_net monitor_cordic/rst_phase monitor_phase_reset/Res
# Inspect reset isolation by leaf.
set reset_net [get_bd_nets -of_objects [get_bd_pins monitor_phase_reset/Res]]
foreach pin [get_bd_pins -of_objects $reset_net] {
    if {[regexp {(^|/)(corrector[01]|cordic[01])(/|$)} $pin]} {error "Monitor reset reaches feedback: $pin"}
}
foreach i {0 1} {
    foreach {cell property expected} {
        complex_mult OutputWidth 24
        boxcar0 DATA_WIDTH 24
        boxcar1 DATA_WIDTH 24
        phase_extractor INPUT_WIDTH 24
        phase_extractor PHASE_WIDTH 24
        phase_extractor RESIDUAL_CORRECTION 1
        phase_unwrapper DIN_WIDTH 24
        phase_unwrapper DOUT_WIDTH 40
    } {
        if {[get_property CONFIG.$property [get_bd_cells cordic$i/$cell]] != $expected} {
            error "Unexpected feedback setting: cordic$i/$cell $property"
        }
    }
    same_net corrector$i/phase_in cordic$i/phase_feedback
    same_net corrector$i/freq_in cordic$i/freq_feedback
    if {[get_property CONFIG.PHASE_FRACTION_BITS [get_bd_cells corrector$i]] != 8} {
        error "Feedback controller must retain eight phase fractional bits"
    }
    if {[get_property CONFIG.GAIN_STAGES [get_bd_cells corrector$i]] != 2} {
        error "Feedback gain latency changed"
    }
    if {[get_property CONFIG.PIPELINED_OVERFLOW [get_bd_cells cordic$i/phase_unwrapper]] != 0} {
        error "Feedback unwrapper behavior changed"
    }
}
puts "PASS: shared 24-bit PNA monitor, fixed /2, 143 MHz CIC/FIR/packets, cyclic DMA and isolated 24-bit feedback"
close_project
