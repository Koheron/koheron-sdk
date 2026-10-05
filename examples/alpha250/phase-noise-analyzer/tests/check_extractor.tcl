# Shared assertions for every PNA board; source after opening system.bd.
proc pna_same_net {left right} {
    set a [get_bd_nets -boundary_type both -of_objects [get_bd_pins $left]]
    set b [get_bd_nets -boundary_type both -of_objects [get_bd_pins $right]]
    foreach net $a {if {[lsearch -exact $b $net] >= 0} {return}}
    error "Missing phase-extractor connection $left -> $right"
}
proc check_phase_extractor {path} {
    foreach {cell property expected} {
        complex_mult OutputWidth 24
        prefilter0 WIDTH 24
        prefilter1 WIDTH 24
        cordic Input_Width 24
        cordic Output_Width 24
        phase_unwrapper DIN_WIDTH 16
        phase_unwrapper DOUT_WIDTH 64
    } {
        if {[get_property CONFIG.$property [get_bd_cells $path/$cell]] != $expected} {
            error "Wrong phase-extractor precision: $path/$cell $property"
        }
    }
    for {set i 0} {$i < 2} {incr i} {
        set data_slice $path/slice_[expr 23+24*$i]_[expr 24*$i]_ccomplex_mult_m_axis_dout_tdata
        pna_same_net $path/complex_mult/m_axis_dout_tdata $data_slice/Din
        pna_same_net $data_slice/Dout $path/prefilter$i/din
        pna_same_net $path/prefilter$i/dout $path/concat_dout_dout/In$i
        pna_same_net $path/prefilter$i/dout $path/slice_23_8_cprefilter${i}_dout/Din
        pna_same_net $path/slice_23_8_cprefilter${i}_dout/Dout $path/demod_quantized/In$i
    }
    pna_same_net $path/concat_dout_dout/dout $path/cordic/s_axis_cartesian_tdata
    pna_same_net $path/demod_quantized/dout $path/demod
    pna_same_net $path/cordic/m_axis_dout_tdata $path/slice_47_24_ccordic_m_axis_dout_tdata/Din
    pna_same_net $path/slice_47_24_ccordic_m_axis_dout_tdata/Dout $path/phase_round/phase_in
    pna_same_net $path/phase_round/phase_out $path/phase_unwrapper/phase_in
}
