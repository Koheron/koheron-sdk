source boards/$board_name/base_system.tcl

# Connect DAC1 to config and ADC1 to status
#connect_pins $status_name/In$config::adc1_offset adc_dac/adc1

# Add PID controller
source $lib/pid_controller.tcl
set pid_out_width 32
add_pid pid_controller [expr $config::adc_width + 1] $pid_out_width
connect_pins pid_controller/clk $adc_clk

foreach {name} {p i d} {
  connect_pins pid_controller/coef_$name [cfg_pin coef_$name]
}
connect_pins pid_controller/integral_reset [cfg_pin integral_reset]

# Connect output of PID controller to DAC2
cell xilinx.com:ip:xlslice:1.0 slice_pid_cmd_out {
  DIN_FROM [expr $pid_out_width - 1]
  DIN_TO [expr $pid_out_width - $config::dac_width]
} {
  Din pid_controller/cmd_out
  Dout adc_dac/dac2
}

# error_in = set_point - adc2
cell xilinx.com:ip:c_addsub:12.0 set_point_minus_signal {
  Add_Mode Subtract
  CE false
  Latency 1
  Out_Width [expr $config::adc_width + 1]
} {
  clk $adc_clk
  A [cfg_pin set_point]
  B adc_dac/adc2
  S pid_controller/error_in
}

# Connect error_in and cmd_out to status
connect_pins set_point_minus_signal/B [sts_pin error_in]
connect_pins pid_controller/cmd_out [sts_pin cmd_out]


# Add DDS
set dds_name dds
cell xilinx.com:ip:dds_compiler:6.0 $dds_name {
 DDS_Clock_Rate 125
 Parameter_Entry Hardware_Parameters
 Phase_Width 32
 Output_Width 24
 Phase_Increment Streaming
 Has_Phase_Out false
 Noise_Shaping Taylor_Series_Corrected
} {
  aclk $adc_clk
}

# Control phase increment

cell pavel-demin:user:axis_constant:1.0 phase_inc {
  AXIS_TDATA_WIDTH 32
} {
  aclk $adc_clk
  M_AXIS $dds_name/S_AXIS_PHASE
  cfg_data [cfg_pin dds]
}

# Create axis_lfsr
cell pavel-demin:user:axis_lfsr:1.0 lfsr_0 {} {
  aclk $adc_clk
  aresetn $rst_adc_clk_name/peripheral_aresetn
}

# Create cmpy
cell xilinx.com:ip:cmpy:6.0 mult_0 {
  APORTWIDTH 14
  BPORTWIDTH 24
  ROUNDMODE Random_Rounding
  OUTPUTWIDTH 25
  LatencyConfig Manual
  MinimumLatency 2
} {
  s_axis_a_tdata adc_dac/adc1
  S_AXIS_B $dds_name/M_AXIS_DATA
  S_AXIS_CTRL lfsr_0/M_AXIS
  aclk $adc_clk
}

connect_constant mult_valid 1 1 mult_0/s_axis_a_tvalid

# Create axis_broadcaster
cell xilinx.com:ip:axis_broadcaster:1.1 bcast_0 {
  S_TDATA_NUM_BYTES.VALUE_SRC USER
  M_TDATA_NUM_BYTES.VALUE_SRC USER
  S_TDATA_NUM_BYTES 8
  M_TDATA_NUM_BYTES 3
  M00_TDATA_REMAP {tdata[23:0]}
  M01_TDATA_REMAP {tdata[55:32]}
} {
  S_AXIS mult_0/M_AXIS_DOUT
  aclk $adc_clk
  aresetn $rst_adc_clk_name/peripheral_aresetn
}

# Create axis_variable
cell pavel-demin:user:axis_variable:1.0 rate_0 {
  AXIS_TDATA_WIDTH 16
} {
  cfg_data [cfg_pin cic_rate]
  aclk $adc_clk
  aresetn $rst_adc_clk_name/peripheral_aresetn
}

# Create cic_compiler
cell xilinx.com:ip:cic_compiler:4.0 cic_0 {
  INPUT_DATA_WIDTH.VALUE_SRC USER
  FILTER_TYPE Decimation
  NUMBER_OF_STAGES 6
  SAMPLE_RATE_CHANGES Programmable
  MINIMUM_RATE 4
  MAXIMUM_RATE 8192
  FIXED_OR_INITIAL_RATE 512
  INPUT_SAMPLE_FREQUENCY 125
  CLOCK_FREQUENCY 125
  INPUT_DATA_WIDTH 24
  QUANTIZATION Truncation
  OUTPUT_DATA_WIDTH 24
  USE_XTREME_DSP_SLICE false
  HAS_DOUT_TREADY true
  HAS_ARESETN true
} {
  S_AXIS_DATA bcast_0/M00_AXIS
  S_AXIS_CONFIG rate_0/M_AXIS
  aclk $adc_clk
  aresetn $rst_adc_clk_name/peripheral_aresetn
}

set intercon_idx 1
set idx 00
cell xilinx.com:ip:axis_clock_converter:1.1 clock_converter {} {
  S_AXIS cic_0/M_AXIS_DATA
  m_axis_aresetn [set rst${intercon_idx}_name]/peripheral_aresetn
  s_axis_aresetn $rst_adc_clk_name/peripheral_aresetn
  s_axis_aclk $adc_clk
  m_axis_aclk [set ps_clk$intercon_idx]
}

cell xilinx.com:ip:axi_fifo_mm_s:4.1 axis_fifo {
  C_USE_TX_DATA 0
  C_USE_TX_CTRL 0
  C_USE_RX_CUT_THROUGH true
  C_RX_FIFO_DEPTH 8192
  C_RX_FIFO_PF_THRESHOLD 4096
} {
  s_axi_aclk [set ps_clk$intercon_idx]
  s_axi_aresetn [set rst${intercon_idx}_name]/peripheral_aresetn
  S_AXI [set interconnect_${intercon_idx}_name]/M${idx}_AXI
  AXI_STR_RXD clock_converter/M_AXIS
}

assign_bd_address [get_bd_addr_segs axis_fifo/S_AXI/Mem0]
set memory_segment [get_bd_addr_segs /${::ps_name}/Data/SEG_axis_fifo_Mem0]
set_property offset $config::axi_fifo_offset $memory_segment
set_property range $config::axi_fifo_range $memory_segment

