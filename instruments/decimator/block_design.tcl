source $board_path/config/ports.tcl

# Add PS and AXI Interconnect
set board_preset $board_path/config/board_preset.tcl
source $lib_path/starting_point.tcl

# Add ADCs and DACs
source $lib_path/redp_adc_dac.tcl
set adc_dac_name adc_dac
add_redp_adc_dac $adc_dac_name

# Rename clocks
set adc_clk $adc_dac_name/adc_clk

# Add processor system reset synchronous to adc clock
set rst_adc_clk_name proc_sys_reset_adc_clk
cell xilinx.com:ip:proc_sys_reset:5.0 $rst_adc_clk_name {} {
  ext_reset_in $ps_name/FCLK_RESET0_N
  slowest_sync_clk $adc_clk
}

# Add config and status registers
source $lib_path/ctl_sts.tcl
add_ctl_sts $adc_clk $rst_adc_clk_name/peripheral_aresetn

# Connect LEDs
connect_port_pin led_o [get_slice_pin [ctl_pin led] 7 0]

# Connect ADC to status register
for {set i 0} {$i < [get_parameter n_adc]} {incr i} {
  connect_pins [ctl_pin dac$i] adc_dac/dac[expr $i + 1]
  connect_pins [sts_pin adc$i] adc_dac/adc[expr $i + 1]
}

# Use AXI Stream clock converter (ADC clock -> FPGA clock)
set intercon_idx 0
set idx [add_master_interface $intercon_idx]
cell xilinx.com:ip:axis_clock_converter:1.1 adc_clock_converter {
  TDATA_NUM_BYTES 2
} {
  s_axis_tdata adc_dac/adc1
  s_axis_tvalid [get_constant_pin 1 1]
  s_axis_aresetn $rst_adc_clk_name/peripheral_aresetn
  m_axis_aresetn [set rst${intercon_idx}_name]/peripheral_aresetn
  s_axis_aclk $adc_clk
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
  Input_Sample_Frequency [expr [get_parameter adc_clk] / 1000000.]
  Clock_Frequency [expr [get_parameter fclk0] / 1000000.]
  Input_Data_Width [get_parameter adc_width]
  Quantization Truncation
  Output_Data_Width 32
  Use_Xtreme_DSP_Slice false
} {
  aclk [set ps_clk$intercon_idx]
  S_AXIS_DATA adc_clock_converter/M_AXIS
}

set fir_coeffs [exec python $project_path/fir.py $n_stages $dec_rate $diff_delay print]

cell xilinx.com:ip:fir_compiler:7.2 fir {
  Filter_Type Decimation
  Sample_Frequency [expr [get_parameter adc_clk] / 1000000. / $dec_rate]
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

