if {$argc != 1} {error "Expected the full DPLL project path"}
open_project -read_only [file normalize [lindex $argv 0]]
open_bd_design [get_files */system.bd]
foreach {cell property expected} {
    fir Data_Width 40 fir Output_Width 40 fir Clock_Frequency 143
    axi_dma_0 c_include_sg 1
} {
    if {[get_property CONFIG.$property [get_bd_cells $cell]] != $expected} {
        error "Unexpected monitor setting: $cell $property"
    }
}
foreach i {0 1} {
    foreach {leaf property expected} {
        complex_mult APortWidth 16 complex_mult OutputWidth 24
        prefilter0 WIDTH 24 prefilter1 WIDTH 24
        phase_extractor INPUT_WIDTH 24 phase_extractor PHASE_WIDTH 24
        phase_extractor COMPACT_PREP 0 phase_extractor RESIDUAL_CORRECTION 1
        phase_unwrapper DIN_WIDTH 24 phase_unwrapper DOUT_WIDTH 64
        phase_unwrapper PIPELINED_OVERFLOW 1
        phase_unwrapper PIPELINED_HISTORY 1
        phase_unwrapper FUSED_DIFFERENCE 1
        phase_unwrapper CANONICAL_INPUT 1
    } {
        if {[get_property CONFIG.$property [get_bd_cells cordic$i/$leaf]] ne $expected} {
            error "Unexpected shared extractor setting: cordic$i/$leaf $property"
        }
    }
    foreach {property expected} {PHASE_FRAC 8 FREQ_WIDTH 25 PHASE_WIDTH 40 GAIN_STAGES 4 FAST_GAIN_STAGES 3 FINAL_CSA_LEVELS 2 CARRY_BLOCK 0 FAST_P_DSP 1 PIPELINED_REFERENCE 1 PRECOMBINE_I 1 SELECTOR_CARRY_BLOCK 0} {
        if {[get_property CONFIG.$property [get_bd_cells corrector$i]] != $expected} {
            error "Incorrect fractional controller interface: $i $property"
        }
    }
}
if {[llength [get_bd_cells -quiet monitor_cordic]]} {error "Dedicated monitor extractor remains"}
if {[llength [get_bd_cells -hier -filter {VLNV =~ xilinx.com:ip:cmpy:*}]] != 2} {
    error "Expected exactly two shared I/Q mixers"
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
foreach i {0 1} {
    same_net cordic$i/aclk adc_dac/adc_clk
    same_net cordic$i/phase corrector$i/phase_in
    same_net cordic$i/freq corrector$i/freq_in
}
same_net monitor_origin/resetn phase_stream_control/filter_resetn
set reset_net [get_bd_nets -of_objects [get_bd_pins monitor_origin/resetn]]
foreach pin [get_bd_pins -of_objects $reset_net] {
    if {[regexp {(^|/)(corrector[01]|cordic[01])(/|$)} $pin]} {error "Monitor reset reaches feedback: $pin"}
}
puts "PASS: two shared accurate extractors, fractional feedback, independent monitor epoch, 143 MHz filtering and cyclic DMA"
close_project
