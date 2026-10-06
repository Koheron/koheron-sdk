namespace eval monitor_filter {

proc create_cic {name} {
    cell xilinx.com:ip:cic_compiler:4.0 $name {
        Filter_Type Decimation
        Number_Of_Stages [get_parameter cic_n_stages]
        Fixed_Or_Initial_Rate [get_parameter cic_decimation_rate_default]
        Sample_Rate_Changes Programmable
        Minimum_Rate [get_parameter cic_decimation_rate_min]
        Maximum_Rate [get_parameter cic_decimation_rate_max]
        Differential_Delay [get_parameter cic_differential_delay]
        Input_Sample_Frequency [expr {[get_parameter adc_clk] / 1000000.}]
        Clock_Frequency [expr {[get_parameter adc_clk] / 1000000.}]
        Input_Data_Width 32
        Quantization Truncation
        Output_Data_Width 32
        Use_Xtreme_DSP_Slice true
        HAS_DOUT_TREADY true
    } {}
}

proc create_fir {name coeffs} {
    # Size capture throughput for the fastest programmable CIC rate.
    set sample_mhz [expr {[get_parameter adc_clk] / 1000000. / [get_parameter cic_decimation_rate_min]}]
    set clock_mhz [expr {[get_parameter adc_clk] / 1000000.}]
    cell xilinx.com:ip:fir_compiler:7.2 $name {
        Filter_Type Decimation
        Sample_Frequency $sample_mhz
        Clock_Frequency $clock_mhz
        Coefficient_Width 32
        Data_Width 32
        Output_Rounding_Mode Convergent_Rounding_to_Even
        Output_Width 32
        Decimation_Rate 2
        BestPrecision true
        CoefficientVector [subst {{$coeffs}}]
        M_DATA_Has_TREADY true
    } {}
}

}
