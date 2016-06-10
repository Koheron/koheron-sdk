# Improve timing on BRAM interconnect
set_property -dict [list CONFIG.S00_HAS_REGSLICE {1}] [get_bd_cells axi_mem_intercon_1]

# shift address/tvalid to take into account demod_data bram read latency
cell xilinx.com:ip:c_shift_ram:12.0 shift_tvalid {
  Width 1
  Depth 1
} {
  CLK $adc_clk
  D $address_name/tvalid
}

# Add spectrum IP
source projects/spectrum_module/spectrum.tcl

set spectrum_name spectrum_0
set spectrum_bram_name spectrum_bram
set n_pts_fft [expr 2**$config::bram_addr_width]
add_spectrum $spectrum_name $n_pts_fft $config::adc_width

for {set i 1} {$i < 3} {incr i} {
  connect_pins spectrum_0/adc$i adc_dac/adc$i
}

connect_pins $spectrum_name/clk        $adc_clk
connect_pins $spectrum_name/tvalid     shift_tvalid/Q
connect_pins $spectrum_name/cfg_sub    [cfg_pin substract_mean]
connect_pins $spectrum_name/cfg_fft    [cfg_pin cfg_fft]

# Add spectrum BRAM
set spectrum_bram_name spectrum_bram
add_bram $spectrum_bram_name $config::axi_spectrum_range $config::axi_spectrum_offset
connect_pins blk_mem_gen_$spectrum_bram_name/clkb $adc_clk
connect_pins blk_mem_gen_$spectrum_bram_name/rstb $rst_adc_clk_name/peripheral_reset
connect_constant const_${spectrum_bram_name}_enb 1 1 blk_mem_gen_$spectrum_bram_name/enb

# Add demod BRAM
set demod_bram_name    demod_bram
add_bram $demod_bram_name $config::axi_demod_range $config::axi_demod_offset
connect_pins blk_mem_gen_$demod_bram_name/clkb  $adc_clk
connect_pins blk_mem_gen_$demod_bram_name/rstb  $rst_adc_clk_name/peripheral_reset
connect_pins blk_mem_gen_$demod_bram_name/doutb $spectrum_name/demod_data
connect_pins blk_mem_gen_$demod_bram_name/addrb $address_name/addr
connect_constant const_${demod_bram_name}_dinb 0 32 blk_mem_gen_$demod_bram_name/dinb
connect_constant const_${demod_bram_name}_enb 1 1  blk_mem_gen_$demod_bram_name/enb
connect_constant const_${demod_bram_name}_web 0 4  blk_mem_gen_$demod_bram_name/web

# Substract noise floor
source projects/spectrum/noise_floor.tcl
set subtract_name noise_floor
add_noise_floor $subtract_name $config::bram_addr_width $adc_clk
connect_pins $subtract_name/clk $adc_clk
connect_pins $subtract_name/s_axis_tdata $spectrum_name/m_axis_result_tdata
connect_pins $subtract_name/s_axis_tvalid $spectrum_name/m_axis_result_tvalid

# Add averaging module
source projects/averager_module/averager.tcl
set avg_name avg
add_averager_module $avg_name $config::bram_addr_width

connect_pins $avg_name/clk         $adc_clk
connect_pins $avg_name/restart     $address_name/restart
connect_pins $avg_name/avg_on      [cfg_pin avg_on]
connect_pins $avg_name/period      [cfg_pin period0]
connect_pins $avg_name/threshold   [cfg_pin threshold0]
connect_pins $avg_name/n_avg_min   [cfg_pin n_avg_min0]

connect_pins $subtract_name/m_axis_result_tdata  $avg_name/din
connect_pins $subtract_name/m_axis_result_tvalid $avg_name/tvalid

connect_pins $avg_name/addr        blk_mem_gen_$spectrum_bram_name/addrb
connect_pins $avg_name/dout        blk_mem_gen_$spectrum_bram_name/dinb
connect_pins $avg_name/wen         blk_mem_gen_$spectrum_bram_name/web

connect_pins $avg_name/n_avg      [sts_pin n_avg]
connect_pins $avg_name/ready      [sts_pin avg_ready]
connect_pins $avg_name/avg_on_out [sts_pin avg_on_out]

# Add peak detector

source projects/peak_detector_module/peak_detector.tcl
set peak_detector_name peak
add_peak_detector $peak_detector_name $config::bram_addr_width

connect_pins $peak_detector_name/clk $adc_clk
connect_pins $peak_detector_name/din $subtract_name/m_axis_result_tdata
connect_pins $peak_detector_name/s_axis_tvalid $subtract_name/m_axis_result_tvalid

connect_pins $peak_detector_name/address_low [cfg_pin peak_address_low]
connect_pins $peak_detector_name/address_high [cfg_pin peak_address_high]
connect_pins $peak_detector_name/address_reset [cfg_pin peak_address_reset]

connect_pins $peak_detector_name/address_out [sts_pin peak_address]
connect_pins $peak_detector_name/maximum_out [sts_pin peak_maximum]

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



