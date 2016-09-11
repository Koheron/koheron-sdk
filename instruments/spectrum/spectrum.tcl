# Improve timing on BRAM interconnect
set_property -dict [list CONFIG.S00_HAS_REGSLICE {1}] [get_bd_cells axi_mem_intercon_1]

# Add spectrum IP
source fpga/modules/spectrum/spectrum.tcl

set spectrum_name spectrum_0
set n_pts_fft [expr 2**$config::bram_addr_width]
spectrum::create $spectrum_name $n_pts_fft $config::adc_width

for {set i 1} {$i < 3} {incr i} {
  connect_pins spectrum_0/adc$i adc_dac/adc$i
}

# shift address/tvalid to take into account demod_data bram read latency
connect_cell $spectrum_name {
  clk        $adc_clk
  tvalid     [get_Q_pin $address_name/tvalid 1 noce $adc_clk]
  cfg_sub    [cfg_pin substract_mean]
  cfg_fft    [cfg_pin cfg_fft]
}

# Add spectrum recorder
source $lib/bram_recorder.tcl
set recorder_name spectrum_recorder
add_bram_recorder $recorder_name spectrum
connect_pins $recorder_name/clk   $adc_clk
connect_pins $recorder_name/rst   $rst_adc_clk_name/peripheral_reset

# Add demod BRAM
set demod_bram_name    demod_bram
add_bram $demod_bram_name $config::axi_demod_range $config::axi_demod_offset

connect_cell blk_mem_gen_$demod_bram_name {
  clkb  $adc_clk
  rstb  $rst_adc_clk_name/peripheral_reset
  doutb $spectrum_name/demod_data
  addrb $address_name/addr0
  dinb  [get_constant_pin 0 32]
  enb   [get_constant_pin 1 1]
  web   [get_constant_pin 0 4]
}

# Substract noise floor
source projects/spectrum/noise_floor.tcl
set subtract_name noise_floor
add_noise_floor $subtract_name $config::bram_addr_width $adc_clk

connect_cell $subtract_name {
  clk $adc_clk
  s_axis_tdata $spectrum_name/m_axis_result_tdata
  s_axis_tvalid $spectrum_name/m_axis_result_tvalid 
}

# Add averaging module
source fpga/modules/averager/averager.tcl
set avg_name avg0
averager::create $avg_name $config::bram_addr_width

connect_cell $avg_name {
  clk         $adc_clk
  restart     $address_name/restart
  avg_on      [cfg_pin avg]
  period      [cfg_pin avg_period]
  threshold   [cfg_pin avg_threshold]
  n_avg_min   [cfg_pin n_avg_min]
  addr        $recorder_name/addr
  dout        $recorder_name/adc
  wen         $recorder_name/wen
  n_avg       [sts_pin n_avg]
  ready       [sts_pin avg_ready]
  avg_on_out  [sts_pin avg_on_out]
}

connect_cell $subtract_name {
  m_axis_result_tdata  $avg_name/din
  m_axis_result_tvalid $avg_name/tvalid 
}

# Add peak detector

source fpga/modules/peak_detector/peak_detector.tcl
set peak_detector_name peak
peak_detector::create $peak_detector_name $config::bram_addr_width

connect_cell $peak_detector_name {
  clk $adc_clk
  din $subtract_name/m_axis_result_tdata
  s_axis_tvalid $subtract_name/m_axis_result_tvalid
  address_low [cfg_pin peak_address_low]
  address_high [cfg_pin peak_address_high]
  address_reset [cfg_pin peak_address_reset]
  address_out [sts_pin peak_address]
  maximum_out [sts_pin peak_maximum]
}

set intercon_idx 0
set idx [add_master_interface $intercon_idx]
cell xilinx.com:ip:axis_clock_converter:1.1 peak_clock_converter {
  TDATA_NUM_BYTES 4
} {
  s_axis_tdata $peak_detector_name/address_out
  s_axis_tvalid $peak_detector_name/m_axis_tvalid
  s_axis_aresetn $rst_adc_clk_name/peripheral_aresetn
  m_axis_aresetn [set rst${intercon_idx}_name]/peripheral_aresetn
  s_axis_aclk $adc_clk
  m_axis_aclk [set ps_clk$intercon_idx]
}

cell xilinx.com:ip:axi_fifo_mm_s:4.1 peak_axis_fifo {
  C_USE_TX_DATA 0
  C_USE_TX_CTRL 0
  C_USE_RX_CUT_THROUGH true
  C_RX_FIFO_DEPTH 4096
  C_RX_FIFO_PF_THRESHOLD 2048
} {
  s_axi_aclk [set ps_clk$intercon_idx]
  s_axi_aresetn [set rst${intercon_idx}_name]/peripheral_aresetn
  S_AXI [set interconnect_${intercon_idx}_name]/M${idx}_AXI
  AXI_STR_RXD peak_clock_converter/M_AXIS
}

assign_bd_address [get_bd_addr_segs peak_axis_fifo/S_AXI/Mem0]
set memory_segment [get_bd_addr_segs /${::ps_name}/Data/SEG_peak_axis_fifo_Mem0]
set_property offset $config::axi_peak_fifo_offset $memory_segment
set_property range $config::axi_peak_fifo_range $memory_segment



