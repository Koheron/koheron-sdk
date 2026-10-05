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
for {set i 0} {$i < 2} {incr i} {
    set c cordic$i
    if {[get_property CONFIG.Output_Width [get_bd_cells $c/cordic]] != 24} {error "Wrong CORDIC width"}
    if {[get_property CONFIG.DOUT_WIDTH [get_bd_cells $c/phase_unwrapper]] != 32} {error "Wrong accumulator width"}
    same_net $c/phase_round/phase_out $c/phase_unwrapper/phase_in
    same_net $c/phase_lfsr/m_axis_tdata $c/slice_7_0_cphase_lfsr_m_axis_tdata/Din
    same_net $c/slice_7_0_cphase_lfsr_m_axis_tdata/Dout $c/phase_round/random_round
    same_net $c/cordic/m_axis_dout_tdata $c/slice_47_24_ccordic_m_axis_dout_tdata/Din
    same_net $c/slice_47_24_ccordic_m_axis_dout_tdata/Dout $c/phase_round/phase_in
    same_net dac_scale$i/dout adc_dac/dac[expr {$i + 1}]
    same_net awg/dac${i}_data slice_15_3_cawg_dac${i}_data/Din
    same_net slice_15_3_cawg_dac${i}_data/Dout dac_scale$i/In0
    same_net dds$i/m_axis_data_tdata $c/s_axis_data_b
}
puts "PASS: RP 24-bit phase extraction, independent rounding streams and DAC/reference separation"
close_project
