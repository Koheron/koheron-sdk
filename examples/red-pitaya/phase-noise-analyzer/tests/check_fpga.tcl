# Run after make xpr, passing the analyzer .xpr as the first argument.
open_project [lindex $argv 0]
open_bd_design [get_files */system.bd]
validate_bd_design
proc same_net {a b} {
    set left [get_bd_nets -boundary_type both -of_objects [get_bd_pins $a]]
    set right [get_bd_nets -boundary_type both -of_objects [get_bd_pins $b]]
    foreach net $left {if {[lsearch -exact $right $net] >= 0} {return}}
    error "Missing connection $a -> $b"
}
set c cordic0
if {[get_property CONFIG.Output_Width [get_bd_cells $c/cordic]] != 24} {error "Wrong CORDIC width"}
if {[get_property CONFIG.DOUT_WIDTH [get_bd_cells $c/phase_unwrapper]] != 64} {error "Wrong accumulator width"}
same_net $c/phase_round/phase_out $c/phase_unwrapper/phase_in
same_net $c/phase_lfsr/m_axis_tdata $c/slice_7_0_cphase_lfsr_m_axis_tdata/Din
same_net $c/slice_7_0_cphase_lfsr_m_axis_tdata/Dout $c/phase_round/random_round
same_net $c/cordic/m_axis_dout_tdata $c/slice_47_24_ccordic_m_axis_dout_tdata/Din
same_net $c/slice_47_24_ccordic_m_axis_dout_tdata/Dout $c/phase_round/phase_in
for {set i 0} {$i < 2} {incr i} {
    same_net dac_scale$i/dout adc_dac/dac[expr {$i + 1}]
    same_net awg/dac${i}_data slice_15_3_cawg_dac${i}_data/Din
    same_net slice_15_3_cawg_dac${i}_data/Dout dac_scale$i/In0
    same_net dds$i/m_axis_data_tdata reference_mux_inputs/In$i
    same_net adc_dac/adc[expr {$i + 1}] concat_dout_adc[expr {$i + 1}]_dout/In1
    same_net concat_dout_adc[expr {$i + 1}]_dout/dout adc_mux_inputs/In$i
}
if {[llength [get_bd_cells -quiet cordic1]]} {error "RP must share the selected phase extractor"}
same_net dds0/m_axis_data_tvalid cordic0/s_axis_tvalid
same_net adc_mux/dout cordic0/s_axis_data_a
same_net reference_mux/dout cordic0/s_axis_data_b
same_net accumulate_mux/dout cordic0/acc_on
same_net cordic0/phase phase_range/din
same_net cordic0/overflow phase_overflow/Op1
foreach mux {adc_mux reference_mux accumulate_mux} {
    same_net slice_4_4_c_ctl_cordic/Dout $mux/sel
}
if {![get_property CONFIG.Use_Xtreme_DSP_Slice [get_bd_cells cic]]} {error "RP CIC must use DSPs"}
source [file join [file dirname [info script]] ../../../alpha250/phase-noise-analyzer/tests/check_dma.tcl]
puts "PASS: RP 24-bit phase extraction, selected ADC/reference muxes and DAC/reference separation"
close_project
