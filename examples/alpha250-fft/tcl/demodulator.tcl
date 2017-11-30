namespace eval demodulator {

proc pins {cmd} {
  $cmd -dir I -type clk      aclk
  $cmd -dir I -from 0  -to 0 aresetn
  $cmd -dir I -from 31 -to 0 s_axis_data_a
  $cmd -dir I -from 31 -to 0 s_axis_data_b
  $cmd -dir I -from 0  -to 0 s_axis_tvalid
  $cmd -dir O -from 31 -to 0 m_axis_tdata
  $cmd -dir O -from 0  -to 0 m_axis_tvalid
}

proc create {module_name} {

    set bd [current_bd_instance .]
    current_bd_instance [create_bd_cell -type hier $module_name]

    pins create_bd_pin

    # Complex multiplier

    cell xilinx.com:ip:cmpy:6.0 complex_mult {
        APortWidth 16
        BPortWidth 16
        OutputWidth 24
    } {
        aclk aclk
        s_axis_a_tdata s_axis_data_a
        s_axis_a_tvalid s_axis_tvalid
        s_axis_b_tdata s_axis_data_b
        s_axis_b_tvalid s_axis_tvalid
    }

    cell xilinx.com:ip:axis_broadcaster:1.1 axis_broadcaster_0 {
        S_TDATA_NUM_BYTES 6
        M_TDATA_NUM_BYTES 3
        M00_TDATA_REMAP {tdata[23:0]}
        M01_TDATA_REMAP {tdata[47:24]}
    } {
        S_AXIS complex_mult/M_AXIS_DOUT
        aclk aclk
        aresetn aresetn
    }

    # Define CIC parameters

    set diff_delay [get_parameter cic_differential_delay]
    set dec_rate [get_parameter cic_decimation_rate]
    set n_stages [get_parameter cic_n_stages]

    for {set i 0} {$i < 2} {incr i} {
        cell xilinx.com:ip:cic_compiler:4.0 cic_$i {
            Filter_Type Decimation
            Number_Of_Stages $n_stages
            Fixed_Or_Initial_Rate $dec_rate
            Differential_Delay $diff_delay
            Input_Sample_Frequency [expr [get_parameter adc_clk] / 1000000.]
            Clock_Frequency [expr [get_parameter adc_clk] / 1000000.]
            Quantization Truncation
            Input_Data_Width 24
            Output_Data_Width 32
            Use_Xtreme_DSP_Slice false
        } {
            aclk aclk
            S_AXIS_DATA axis_broadcaster_0/M0${i}_AXIS
        }
    }

    # FIR

    cell xilinx.com:ip:axis_combiner:1.1 axis_combiner_0 {
        TDATA_NUM_BYTES 4
    } {
        aclk aclk
        aresetn aresetn
        S00_AXIS cic_0/M_AXIS_DATA
        S01_AXIS cic_1/M_AXIS_DATA
    }

    cell xilinx.com:ip:axis_dwidth_converter:1.1 axis_dwidth_converter_0 {
        S_TDATA_NUM_BYTES 8
        M_TDATA_NUM_BYTES 4
    } {
        S_AXIS axis_combiner_0/M_AXIS
        aclk aclk
        aresetn aresetn
    }

    set fir_coeffs [exec python $::project_path/fir.py $n_stages $dec_rate $diff_delay print]

    cell xilinx.com:ip:fir_compiler:7.2 fir {
        Filter_Type Decimation
        Sample_Frequency [expr [get_parameter adc_clk] / 1000000. / $dec_rate]
        Clock_Frequency [expr [get_parameter adc_clk] / 1000000.]
        Coefficient_Width 32
        Data_Width 32
        Output_Rounding_Mode Convergent_Rounding_to_Even
        Output_Width 32
        Decimation_Rate 2
        BestPrecision true
        CoefficientVector [subst {{$fir_coeffs}}]
        NUMBER_CHANNELS 2
    } {
        aclk aclk
        S_AXIS_DATA axis_dwidth_converter_0/M_AXIS
        m_axis_data_tdata m_axis_tdata
        m_axis_data_tvalid m_axis_tvalid
    }

  current_bd_instance $bd
}

} ;# end spectrum namespace