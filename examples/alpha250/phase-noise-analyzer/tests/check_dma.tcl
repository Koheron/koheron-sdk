# Shared single-stream DMA assertions. Caller has opened the block design.
proc pna_dma_net {left right} {
  set a [get_bd_nets -boundary_type both -of_objects [get_bd_pins $left]]
  set b [get_bd_nets -boundary_type both -of_objects [get_bd_pins $right]]
  foreach net $a { if {[lsearch -exact $b $net] >= 0} { return } }
  error "Wrong DMA connection: $left -> $right"
}
if {[get_property CONFIG.c_include_sg [get_bd_cells axi_dma_0]] != 1} {error "DMA is not SG"}
if {[get_property CONFIG.PKT_LENGTH [get_bd_cells phase_quantizer]] != 8192} {error "Wrong phase packet size"}
if {[get_property CONFIG.TUSER_WIDTH [get_bd_cells phase_fifo]] != 6} {error "Missing packet metadata"}
set split_filter [expr {[llength [get_bd_cells -quiet phase_fixed_decimator]] > 0}]
pna_dma_net phase_stream_control/filter_resetn phase_range/aresetn
if {$split_filter} {
  pna_dma_net phase_stream_control/filter_resetn phase_fixed_decimator/aresetn
  foreach core {cic fir phase_quantizer} {
    pna_dma_net phase_filter_cdc/resetn $core/aresetn
    pna_dma_net ps_0/FCLK_CLK1 $core/aclk
  }
  pna_dma_net phase_filter_cdc/resetn phase_fifo/s_axis_aresetn
  pna_dma_net phase_stream_control/filter_resetn phase_cic_clock_converter/s_axis_aresetn
  pna_dma_net phase_filter_cdc/resetn phase_cic_clock_converter/m_axis_aresetn
  pna_dma_net phase_stream_control/data_valid phase_fixed_decimator/s_axis_tvalid
  pna_dma_net phase_stream_control/data_ready phase_fixed_decimator/s_axis_tready
  pna_dma_net phase_stream_control/config_rate phase_filter_cdc/rate_in
  pna_dma_net phase_filter_cdc/rate cic/total_rate
  pna_dma_net adc_dac/adc_clk phase_fixed_decimator/aclk
  pna_dma_net adc_dac/adc_clk phase_cic_clock_converter/s_axis_aclk
  pna_dma_net ps_0/FCLK_CLK1 phase_cic_clock_converter/m_axis_aclk
  if {[get_property CONFIG.RATE_STEP [get_bd_cells phase_stream_control]] != 2} {error "Expected even CIC rates"}
  pna_dma_net phase_quantizer/packet_status phase_filter_cdc/packet_in
  pna_dma_net phase_filter_cdc/metadata slice_5_5_cphase_filter_cdc_metadata/Din
  pna_dma_net slice_5_5_cphase_filter_cdc_metadata/Dout sample_metadata/In1
} else {
  if {[llength [get_bd_cells -quiet {phase_filter_cdc phase_cic_clock_converter}]]} {error "Unexpected split-filter clock crossing"}
  if {[get_property VLNV [get_bd_cells cic]] ne "xilinx.com:ip:cic_compiler:4.0"} {error "Expected single programmable CIC"}
  foreach {property expected} {Number_Of_Stages 6 Differential_Delay 1 Input_Data_Width 32 Output_Data_Width 40 Sample_Rate_Changes Programmable Minimum_Rate 4 Maximum_Rate 8192} {
    if {[get_property CONFIG.$property [get_bd_cells cic]] ne $expected} {error "Wrong single CIC $property"}
  }
  foreach core {cic fir phase_quantizer} {
    pna_dma_net phase_stream_control/filter_resetn $core/aresetn
    pna_dma_net adc_dac/adc_clk $core/aclk
  }
  pna_dma_net phase_stream_control/filter_resetn phase_fifo/s_axis_aresetn
  pna_dma_net adc_dac/adc_clk phase_fifo/s_axis_aclk
  pna_dma_net phase_stream_control/data_valid cic/s_axis_data_tvalid
  pna_dma_net phase_stream_control/data_ready cic/s_axis_data_tready
  pna_dma_net phase_stream_control/config_rate cic/s_axis_config_tdata
  pna_dma_net phase_stream_control/config_valid cic/s_axis_config_tvalid
  pna_dma_net phase_stream_control/config_ready cic/s_axis_config_tready
  pna_dma_net phase_stream_control/active_bits phase_quantizer/requested_bits
  pna_dma_net phase_stream_control/overflow phase_quantizer/upstream_overflow
  pna_dma_net phase_stream_control/sample_gap sample_metadata/In1
  if {[get_property CONFIG.RATE_STEP [get_bd_cells phase_stream_control]] != 1} {error "Expected integer CIC rates"}
}
pna_dma_net phase_quantizer/sample_status sample_metadata/In0
pna_dma_net sample_metadata/dout phase_fifo/s_axis_tuser
foreach pair {{phase_fifo/M_AXIS phase_packet_framer/S_AXIS_0} {phase_packet_framer/M_AXIS axi_dma_0/S_AXIS_S2MM}} {
  lassign $pair left right
  set a [get_bd_intf_nets -of_objects [get_bd_intf_pins $left]]
  set b [get_bd_intf_nets -of_objects [get_bd_intf_pins $right]]
  if {[llength $a] != 1 || $a ne $b} {error "Wrong DMA AXIS connection: $left -> $right"}
}
pna_dma_net phase_range/overflow phase_overflow/Op2
pna_dma_net phase_overflow/Res phase_stream_control/upstream_overflow
foreach i {0 1} {
  if {![llength [get_bd_cells -quiet cordic$i]]} {continue}
  if {[get_property CONFIG.DOUT_WIDTH [get_bd_cells cordic$i/phase_unwrapper]] != 64} {error "Narrow accumulator"}
  pna_dma_net phase_history_reset/Res cordic$i/rst_phase
  pna_dma_net cordic$i/rst_phase cordic$i/phase_unwrapper/rst
}
foreach space {Data_S2MM Data_SG} {
  set segment [get_bd_addr_segs axi_dma_0/$space/SEG_ps_0_HP0_DDR_LOWOCM]
  set bytes [expr [string map {K *1024 M *1024*1024} [get_property RANGE $segment]]]
  if {[get_property OFFSET $segment] != 0x1e000000 || $bytes != 0x2000000} {
    error "Incorrect DMA DDR region"
  }
}
puts "PASS: split_filter=$split_filter, cyclic SG DMA, full-history reset and queued precision/gap metadata"
