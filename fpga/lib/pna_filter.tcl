# Shared full-precision fixed /2, asynchronous stream crossing, programmable
# six-stage CIC and PNA compensation FIR. Both single and paired PNA use this.
proc pna_create_filter {suffix fast_clock fast_resetn phase_data data_valid slow_clock slow_mhz slow_resetn slow_rate} {
  if {[get_parameter cic_n_stages] != 6 || [get_parameter cic_differential_delay] != 1 || [get_parameter phase_filter_width] != 40} {
    error "Split phase filter requires six CIC stages, delay one and 40-bit output"
  }
  cell koheron:user:phase_fixed_decimator:1.0 phase_fixed_decimator$suffix {} [list \
    aclk $fast_clock aresetn $fast_resetn s_axis_tdata $phase_data s_axis_tvalid $data_valid]
  cell xilinx.com:ip:axis_clock_converter:1.1 phase_cic_clock_converter$suffix {
    TDATA_NUM_BYTES 5
  } [list S_AXIS phase_fixed_decimator$suffix/M_AXIS \
    s_axis_aclk $fast_clock s_axis_aresetn $fast_resetn \
    m_axis_aclk $slow_clock m_axis_aresetn $slow_resetn]
  cell koheron:user:phase_cic_decimator:1.0 cic$suffix {} [list \
    aclk $slow_clock aresetn $slow_resetn total_rate $slow_rate \
    S_AXIS phase_cic_clock_converter$suffix/M_AXIS]
  global python
  set fir_coeffs [exec -- env -i $python -I fpga/scripts/fir.py \
    [get_parameter cic_n_stages] [get_parameter cic_decimation_rate_min] [get_parameter cic_differential_delay] print]
  cell xilinx.com:ip:fir_compiler:7.2 fir$suffix {
    Filter_Type Decimation
    Sample_Frequency [expr [get_parameter adc_clk] / 1000000. / [get_parameter cic_decimation_rate_min]]
    Clock_Frequency $slow_mhz
    Coefficient_Width 32
    Data_Width 40
    Output_Rounding_Mode Convergent_Rounding_to_Even
    Output_Width 40
    Decimation_Rate 2
    BestPrecision true
    CoefficientVector [subst {{$fir_coeffs}}]
    M_DATA_Has_TREADY true
    Has_ARESETn true
    Reset_Data_Vector true
  } [list aclk $slow_clock aresetn $slow_resetn S_AXIS_DATA cic$suffix/M_AXIS]
  return phase_fixed_decimator$suffix/s_axis_tready
}
