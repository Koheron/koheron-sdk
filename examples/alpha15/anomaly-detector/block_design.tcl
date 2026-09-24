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

##################################################
# DMA
##################################################

# Configure Zynq Processing System
set_cell_props ps_0 {
  PCW_USE_S_AXI_HP2 1
  PCW_S_AXI_HP2_DATA_WIDTH 64
  PCW_USE_HIGH_OCM 1
  PCW_USE_S_AXI_GP0 1
}

connect_pins ps_0/S_AXI_GP0_ACLK ps_0/FCLK_CLK0
connect_pins ps_0/S_AXI_HP2_ACLK ps_0/FCLK_CLK0

cell xilinx.com:ip:axi_interconnect:2.1 dma_interconnect {
  NUM_SI 2
  NUM_MI 2
  S01_HAS_REGSLICE 1
} {
  ACLK ps_0/FCLK_CLK0
  ARESETN proc_sys_reset_0/peripheral_aresetn
  M00_AXI ps_0/S_AXI_GP0
  M01_AXI ps_0/S_AXI_HP2
  S00_ACLK ps_0/FCLK_CLK0
  S00_ARESETN proc_sys_reset_0/peripheral_aresetn
  S01_ACLK ps_0/FCLK_CLK0
  S01_ARESETN proc_sys_reset_0/peripheral_aresetn
  M00_ACLK ps_0/FCLK_CLK0
  M00_ARESETN proc_sys_reset_0/peripheral_aresetn
  M01_ACLK ps_0/FCLK_CLK0
  M01_ARESETN proc_sys_reset_0/peripheral_aresetn
}

# ADC0 is stored as one 32-bit word per 18-bit sample, with a 14-bit
# conversion sequence in the upper bits for loss detection.

cell koheron:user:anomaly_engine:1.0 anomaly_engine_0 {} {
  clk adc_dac/adc_clk
  resetn rst_adc_clk/peripheral_aresetn
  sample adc_dac/adc0
  sample_valid adc_dac/adc_valid
  capture_enable [get_slice_pin [ctl_pin capture_enable] 0 0]
  arm [get_slice_pin [ctl_pin capture_arm] 0 0]
  model_enable [get_slice_pin [ctl_pin model_enable] 0 0]
  threshold [ctl_pin anomaly_threshold]
  commit [get_slice_pin [ctl_pin model_commit] 0 0]
  weight0 [ctl_pin model_weight_packed0]
  weight1 [ctl_pin model_weight_packed1]
  weight2 [ctl_pin model_weight_packed2]
  weight3 [ctl_pin model_weight_packed3]
  bias [ctl_pin model_bias]
  score [sts_pin anomaly_score]
  trigger_sequence [sts_pin anomaly_trigger]
  sequence_status [sts_pin capture_sequence]
}
connect_pins exp_io_b35_0_n anomaly_engine_0/alert
connect_pins [sts_pin anomaly_triggered] [get_concat_pin [list anomaly_engine_0/triggered [get_constant_pin 0 31]]]
connect_pins [sts_pin capture_done] [get_concat_pin [list anomaly_engine_0/capture_done [get_constant_pin 0 31]]]
connect_pins [sts_pin anomaly_alert] [get_concat_pin [list anomaly_engine_0/alert [get_constant_pin 0 31]]]

# A fresh capture starts at a fresh packet boundary. Reset the whole receive
# stream while capture_enable is low, including the TLAST counter.
set stream_adc_resetn [get_and_pin rst_adc_clk/peripheral_aresetn [get_slice_pin [ctl_pin capture_enable] 0 0]]
set stream_ps_resetn [get_and_pin proc_sys_reset_0/peripheral_aresetn [get_slice_pin [ps_ctl_pin capture_reset_ps] 0 0]]

cell xilinx.com:ip:axis_dwidth_converter:1.1 axis_dwidth_converter_0 {
  S_TDATA_NUM_BYTES 4
  M_TDATA_NUM_BYTES 8
} {
  aclk adc_dac/adc_clk
  aresetn $stream_adc_resetn
  s_axis_tdata anomaly_engine_0/tagged_sample
  s_axis_tvalid anomaly_engine_0/tagged_valid
}

# Count ADC words lost when the downstream stream applies backpressure.
cell koheron:user:adc_overflow_counter:1.0 adc_overflow_counter_0 {} {
  clk adc_dac/adc_clk
  resetn rst_adc_clk/peripheral_aresetn
  sample_valid anomaly_engine_0/tagged_valid
  sample_ready axis_dwidth_converter_0/s_axis_tready
  lost_count [sts_pin adc_overflow]
}

cell xilinx.com:ip:axis_clock_converter:1.1 axis_clock_converter_0 {
  TDATA_NUM_BYTES 8
} {
  s_axis_aclk adc_dac/adc_clk
  s_axis_aresetn $stream_adc_resetn
  m_axis_aclk ps_0/FCLK_CLK0
  m_axis_aresetn $stream_ps_resetn
  S_AXIS axis_dwidth_converter_0/M_AXIS
}

cell koheron:user:tlast_gen:1.0 tlast_gen_0 {
  TDATA_WIDTH 64
  PKT_LENGTH [expr 64 * 1024]
} {
  aclk ps_0/FCLK_CLK0
  resetn $stream_ps_resetn
  s_axis axis_clock_converter_0/M_AXIS
}

# DMA

cell xilinx.com:ip:axi_dma:7.1 axi_dma_0 {
  c_include_sg 1
  c_include_mm2s 0
  c_sg_include_stscntrl_strm 0
  c_sg_length_width 20
  c_s2mm_burst_size 16
  c_m_axi_s2mm_data_width 64
} {
  S_AXI_LITE axi_mem_intercon_0/M[add_master_interface]_AXI
  s_axi_lite_aclk ps_0/FCLK_CLK0
  M_AXI_SG dma_interconnect/S00_AXI
  m_axi_sg_aclk ps_0/FCLK_CLK0
  M_AXI_S2MM dma_interconnect/S01_AXI
  m_axi_s2mm_aclk ps_0/FCLK_CLK0
  S_AXIS_S2MM tlast_gen_0/m_axis
  axi_resetn proc_sys_reset_0/peripheral_aresetn
  s2mm_introut [get_interrupt_pin]
}

cell koheron:user:dac_exciter:1.0 dac_exciter_0 {} {
  clk adc_dac/adc_clk
  resetn rst_adc_clk/peripheral_aresetn
  inject_toggle [get_slice_pin [ctl_pin inject_toggle] 0 0]
  dac_data adc_dac/dac0
}
connect_pins adc_dac/dac1 [get_constant_pin 0 16]

# DMA AXI Lite
assign_bd_address [get_bd_addr_segs {axi_dma_0/S_AXI_LITE/Reg }]
set_property range [get_memory_range dma] [get_bd_addr_segs {ps_0/Data/SEG_axi_dma_0_Reg}]
set_property offset [get_memory_offset dma] [get_bd_addr_segs {ps_0/Data/SEG_axi_dma_0_Reg}]

# Scatter Gather interface in On Chip Memory
assign_bd_address [get_bd_addr_segs {ps_0/S_AXI_GP0/GP0_HIGH_OCM }]
set_property range 64K [get_bd_addr_segs {axi_dma_0/Data_SG/SEG_ps_0_GP0_HIGH_OCM}]
set_property offset 0xFFFF0000 [get_bd_addr_segs {axi_dma_0/Data_SG/SEG_ps_0_GP0_HIGH_OCM}]
exclude_bd_addr_seg [get_bd_addr_segs axi_dma_0/Data_S2MM/SEG_ps_0_GP0_HIGH_OCM]

# S2MM on HP2
assign_bd_address [get_bd_addr_segs {ps_0/S_AXI_HP2/HP2_DDR_LOWOCM }]
assign_bd_address -target_address_space /axi_dma_0/Data_S2MM [get_bd_addr_segs ps_0/S_AXI_HP2/HP2_DDR_LOWOCM] -force
set_property range [get_memory_range ram_s2mm] [get_bd_addr_segs {axi_dma_0/Data_S2MM/SEG_ps_0_HP2_DDR_LOWOCM}]
set_property offset [get_memory_offset ram_s2mm] [get_bd_addr_segs {axi_dma_0/Data_S2MM/SEG_ps_0_HP2_DDR_LOWOCM}]
exclude_bd_addr_seg [get_bd_addr_segs axi_dma_0/Data_SG/SEG_ps_0_HP2_DDR_LOWOCM]
validate_bd_design
