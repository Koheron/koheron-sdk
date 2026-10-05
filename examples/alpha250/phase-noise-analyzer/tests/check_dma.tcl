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
foreach core {cic fir phase_quantizer phase_range} {
  pna_dma_net phase_stream_control/filter_resetn $core/aresetn
}
pna_dma_net phase_stream_control/filter_resetn phase_fifo/s_axis_aresetn
pna_dma_net phase_stream_control/data_valid cic/s_axis_data_tvalid
pna_dma_net phase_stream_control/data_ready cic/s_axis_data_tready
pna_dma_net phase_stream_control/config_rate cic/s_axis_config_tdata
pna_dma_net phase_stream_control/config_valid cic/s_axis_config_tvalid
pna_dma_net phase_stream_control/config_ready cic/s_axis_config_tready
pna_dma_net phase_quantizer/sample_status sample_metadata/In0
pna_dma_net phase_stream_control/sample_gap sample_metadata/In1
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
puts "PASS: single-stream cyclic SG DMA, full-history reset, wide phase and queued precision/gap metadata"
