source $board_path/starting_point.tcl

# -----------------------------------------------------------------------------
# ADC
# -----------------------------------------------------------------------------

connect_pins [get_slice_pin [ctl_pin rf_adc_ctl0] 3 3] adc_dac/adc_clkout_dec
connect_pins [get_slice_pin [ctl_pin adp5071_sync] 0 0] adc_dac/adp5071_sync_en
connect_pins [get_slice_pin [ctl_pin adp5071_sync] 1 1] adc_dac/adp5071_sync_state

for {set i 0} {$i < 2} {incr i} {
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 0 0] adc${i}_ctl_range_sel
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 1 1] adc${i}_ctl_testpat
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 2 2] adc${i}_ctl_en

  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 8 4] adc_dac/adc${i}_dco_delay_tap
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 14 9] adc_dac/adc${i}_da_delay_tap
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 20 15] adc_dac/adc${i}_db_delay_tap
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 21 21] adc_dac/adc${i}_delay_reset
}

# Use AXI Stream clock converter (ADC clock -> FPGA clock)
set intercon_idx 0
set idx [add_master_interface $intercon_idx]
cell xilinx.com:ip:axis_clock_converter:1.1 adc_clock_converter {
  TDATA_NUM_BYTES 3
} {
  s_axis_tdata adc_dac/adc0
  s_axis_tvalid adc_dac/adc_valid
  s_axis_aresetn rst_adc_clk/peripheral_aresetn
  m_axis_aresetn [set rst${intercon_idx}_name]/peripheral_aresetn
  s_axis_aclk adc_dac/adc_clk
  m_axis_aclk [set ps_clk$intercon_idx]
}

# Define CIC parameters

set diff_delay [get_parameter cic_differential_delay]
set dec_rate [get_parameter cic_decimation_rate]
set n_stages [get_parameter cic_n_stages]

cell xilinx.com:ip:cic_compiler:4.0 cic {
  Filter_Type Decimation
  Number_Of_Stages $n_stages
  Fixed_Or_Initial_Rate $dec_rate
  Differential_Delay $diff_delay
  Input_Sample_Frequency 15
  Clock_Frequency [expr [get_parameter fclk0] / 1000000.]
  Input_Data_Width [get_parameter adc_width]
  Quantization Truncation
  Output_Data_Width 32
  Use_Xtreme_DSP_Slice false
} {
  aclk [set ps_clk$intercon_idx]
  S_AXIS_DATA adc_clock_converter/M_AXIS
}

set fir_coeffs [exec $python $project_path/fir.py $n_stages $dec_rate $diff_delay print]

cell xilinx.com:ip:fir_compiler:7.2 fir {
  Filter_Type Decimation
  Sample_Frequency [expr 15.0 / $dec_rate]
  Clock_Frequency [expr [get_parameter fclk0] / 1000000.]
  Coefficient_Width 32
  Data_Width 32
  Output_Rounding_Mode Convergent_Rounding_to_Even
  Output_Width 32
  Decimation_Rate 2
  BestPrecision true
  CoefficientVector [subst {{$fir_coeffs}}]
} {
  aclk [set ps_clk$intercon_idx]
  S_AXIS_DATA cic/M_AXIS_DATA
}

# Add AXI stream FIFO
cell xilinx.com:ip:axi_fifo_mm_s:4.1 adc_axis_fifo {
  C_USE_TX_DATA 0
  C_USE_TX_CTRL 0
  C_USE_RX_CUT_THROUGH true
  C_RX_FIFO_DEPTH 16384
  C_RX_FIFO_PF_THRESHOLD 8192
} {
  s_axi_aclk [set ps_clk$intercon_idx]
  s_axi_aresetn [set rst${intercon_idx}_name]/peripheral_aresetn
  S_AXI [set interconnect_${intercon_idx}_name]/M${idx}_AXI
  AXI_STR_RXD fir/M_AXIS_DATA
}

assign_bd_address [get_bd_addr_segs adc_axis_fifo/S_AXI/Mem0]
set memory_segment  [get_bd_addr_segs /${::ps_name}/Data/SEG_adc_axis_fifo_Mem0]
set_property offset [get_memory_offset adc_fifo] $memory_segment
set_property range  [get_memory_range adc_fifo]  $memory_segment
