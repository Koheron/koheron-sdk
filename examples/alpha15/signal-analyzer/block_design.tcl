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
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 21 21] adc_dac/adc${i}_delay_rst
}

# Channel selection / addition / substraction
source $project_path/tcl/adc_selector.tcl
adc_selector::create adc_selector

connect_cell adc_selector {
  adc0           adc_dac/adc0
  adc1           adc_dac/adc1
  offset0        [ctl_pin channel_offset0]
  offset1        [ctl_pin channel_offset1]
  channel_select [ctl_pin channel_select]
  clk            adc_dac/adc_clk
  adc_valid      adc_dac/adc_valid
}

# -----------------------------------------------------------------------------
# Decimation
# -----------------------------------------------------------------------------

# Use AXI Stream clock converter (ADC clock -> PS clock)
set intercon_idx 0
cell xilinx.com:ip:axis_clock_converter:1.1 adc_clock_converter {
  TDATA_NUM_BYTES 3
} {
  s_axis_tdata   adc_selector/tdata
  s_axis_tvalid  adc_selector/tvalid
  s_axis_aresetn rst_adc_clk/peripheral_aresetn
  m_axis_aresetn [set rst${intercon_idx}_name]/peripheral_aresetn
  s_axis_aclk    adc_dac/adc_clk
  m_axis_aclk    [set ps_clk$intercon_idx]
}

# Define CIC parameters

set diff_delay [get_parameter cic_differential_delay]
set dec_rate_default [get_parameter cic_decimation_rate_default]
set dec_rate_min [get_parameter cic_decimation_rate_min]
set dec_rate_max [get_parameter cic_decimation_rate_max]
set n_stages [get_parameter cic_n_stages]

cell xilinx.com:ip:cic_compiler:4.0 cic {
  Filter_Type Decimation
  Number_Of_Stages $n_stages
  Fixed_Or_Initial_Rate $dec_rate_default
  Sample_Rate_Changes Programmable
  Minimum_Rate $dec_rate_min
  Maximum_Rate $dec_rate_max
  Differential_Delay $diff_delay
  Input_Sample_Frequency 15
  Clock_Frequency [expr [get_parameter fclk0] / 1000000.]
  Input_Data_Width [get_parameter adc_width]
  Quantization Truncation
  Output_Data_Width 32
  Use_Xtreme_DSP_Slice false
} {
  aclk        [set ps_clk$intercon_idx]
  S_AXIS_DATA adc_clock_converter/M_AXIS
}

cell pavel-demin:user:axis_variable:1.0 cic_rate {
  AXIS_TDATA_WIDTH 16
} {
  cfg_data [ps_ctl_pin cic_rate]
  aclk [set ps_clk$intercon_idx]
  aresetn [set rst${intercon_idx}_name]/peripheral_aresetn
  M_AXIS cic/S_AXIS_CONFIG
}

set fir_coeffs [exec $python $project_path/fir.py $n_stages $dec_rate_min $diff_delay print]

cell xilinx.com:ip:fir_compiler:7.2 fir {
  Filter_Type Decimation
  Sample_Frequency [expr 15.0 / $dec_rate_min]
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

set idx [add_master_interface $intercon_idx]
# Add AXI stream FIFO
cell xilinx.com:ip:axi_fifo_mm_s:4.1 adc_axis_fifo {
  C_USE_TX_DATA 0
  C_USE_TX_CTRL 0
  C_USE_RX_CUT_THROUGH true
  C_RX_FIFO_DEPTH 16384
  C_RX_FIFO_PF_THRESHOLD 8192
} {
  s_axi_aclk    [set ps_clk$intercon_idx]
  s_axi_aresetn [set rst${intercon_idx}_name]/peripheral_aresetn
  S_AXI         [set interconnect_${intercon_idx}_name]/M${idx}_AXI
  AXI_STR_RXD   fir/M_AXIS_DATA
}

assign_bd_address   [get_bd_addr_segs adc_axis_fifo/S_AXI/Mem0]
set memory_segment  [get_bd_addr_segs /${::ps_name}/Data/SEG_adc_axis_fifo_Mem0]
set_property offset [get_memory_offset adc_fifo] $memory_segment
set_property range  [get_memory_range adc_fifo]  $memory_segment

# -----------------------------------------------------------------------------
# PSD
# -----------------------------------------------------------------------------

source $project_path/tcl/power_spectral_density.tcl
source $sdk_path/fpga/modules/bram_accumulator/bram_accumulator.tcl
source $sdk_path/fpga/lib/bram_recorder.tcl

power_spectral_density::create psd [get_parameter fft_size]

cell xilinx.com:ip:axis_clock_converter:1.1 psd_clock_converter {
  TDATA_NUM_BYTES 3
} {
  s_axis_tdata   adc_selector/tdata
  s_axis_tvalid  adc_selector/tvalid
  s_axis_aresetn rst_adc_clk/peripheral_aresetn
  m_axis_aresetn [set rst${intercon_idx}_name]/peripheral_aresetn
  s_axis_aclk    adc_dac/adc_clk
  m_axis_aclk    [set ps_clk$intercon_idx]
  m_axis_tready  [get_constant_pin 1 1]
}

connect_cell psd {
  data       psd_clock_converter/m_axis_tdata
  clk        [set ps_clk$intercon_idx]
  tvalid     psd_clock_converter/m_axis_tvalid
  ctl_fft    [ps_ctl_pin ctl_fft]
}

# Accumulator
cell koheron:user:psd_counter:1.0 psd_counter {
  PERIOD [get_parameter fft_size]
  PERIOD_WIDTH [expr int(ceil(log([get_parameter fft_size]))/log(2))]
  N_CYCLES [get_parameter n_cycles]
  N_CYCLES_WIDTH [expr int(ceil(log([get_parameter n_cycles]))/log(2))]
} {
  clk           [set ps_clk$intercon_idx]
  s_axis_tdata  psd/m_axis_result_tdata
  s_axis_tvalid psd/m_axis_result_tvalid
  cycle_index   [ps_sts_pin cycle_index]
}

bram_accumulator::create bram_accum
connect_cell bram_accum {
  clk           [set ps_clk$intercon_idx]
  s_axis_tdata  psd_counter/m_axis_tdata
  s_axis_tvalid psd_counter/m_axis_tvalid
  addr_in       psd_counter/addr
  first_cycle   psd_counter/first_cycle
  last_cycle    psd_counter/last_cycle
}

# Record spectrum data in BRAM

add_bram_recorder psd_bram psd
connect_cell psd_bram {
  clk  [set ps_clk$intercon_idx]
  rst  [set rst${intercon_idx}_name]/peripheral_reset
  addr bram_accum/addr_out
  wen  bram_accum/wen
  adc  bram_accum/m_axis_tdata
}