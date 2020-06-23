source $board_path/starting_point.tcl

connect_pins ps_0/SDIO0_CDN [get_constant_pin 0 1]
connect_pins ps_0/SDIO0_WP [get_constant_pin 0 1]

##################################################
# DMA
##################################################

# Configure Zynq Processing System
set_cell_props ps_0 {
  PCW_USE_S_AXI_HP0 1
  PCW_S_AXI_HP0_DATA_WIDTH 64
  PCW_USE_S_AXI_HP2 1
  PCW_S_AXI_HP2_DATA_WIDTH 64
  PCW_USE_HIGH_OCM 1
  PCW_USE_S_AXI_GP0 1
}

connect_pins ps_0/S_AXI_GP0_ACLK ps_0/FCLK_CLK0
connect_pins ps_0/S_AXI_HP0_ACLK ps_0/FCLK_CLK0
connect_pins ps_0/S_AXI_HP2_ACLK ps_0/FCLK_CLK0

cell xilinx.com:ip:axi_interconnect:2.1 dma_interconnect {
  NUM_SI 3
  NUM_MI 3
  S01_HAS_REGSLICE 1
  S02_HAS_REGSLICE 1
} {
  ACLK ps_0/FCLK_CLK0
  ARESETN proc_sys_reset_0/peripheral_aresetn
  M00_AXI ps_0/S_AXI_GP0
  M01_AXI ps_0/S_AXI_HP0
  M02_AXI ps_0/S_AXI_HP2
  S00_ACLK ps_0/FCLK_CLK0
  S00_ARESETN proc_sys_reset_0/peripheral_aresetn
  S01_ACLK ps_0/FCLK_CLK0
  S01_ARESETN proc_sys_reset_0/peripheral_aresetn
  S02_ACLK ps_0/FCLK_CLK0
  S02_ARESETN proc_sys_reset_0/peripheral_aresetn
  M00_ACLK ps_0/FCLK_CLK0
  M00_ARESETN proc_sys_reset_0/peripheral_aresetn
  M01_ACLK ps_0/FCLK_CLK0
  M01_ARESETN proc_sys_reset_0/peripheral_aresetn
  M02_ACLK ps_0/FCLK_CLK0
  M02_ARESETN proc_sys_reset_0/peripheral_aresetn
}

# ADC Streaming (S2MM)

cell koheron:user:bus_multiplexer:1.0 adc_mux {
  WIDTH 16
} {
  din0 adc_dac/adc0
  din1 adc_dac/adc1
  sel [get_slice_pin [ctl_pin channel_select] 0 0]
}

cell xilinx.com:ip:axis_dwidth_converter:1.1 axis_dwidth_converter_0 {
  S_TDATA_NUM_BYTES 2
  M_TDATA_NUM_BYTES 8
} {
  aclk adc_dac/adc_clk
  aresetn rst_adc_clk/peripheral_aresetn
  s_axis_tdata adc_mux/dout
  s_axis_tvalid axis_dwidth_converter_0/s_axis_tready
}

cell xilinx.com:ip:axis_clock_converter:1.1 axis_clock_converter_0 {
  TDATA_NUM_BYTES 8
} {
  s_axis_aclk adc_dac/adc_clk
  s_axis_aresetn rst_adc_clk/peripheral_aresetn
  m_axis_aclk ps_0/FCLK_CLK0
  m_axis_aresetn proc_sys_reset_0/peripheral_aresetn
  S_AXIS axis_dwidth_converter_0/M_AXIS
}

cell koheron:user:tlast_gen:1.0 tlast_gen_0 {
  TDATA_WIDTH 64
  PKT_LENGTH [expr 1024 * 1024]
} {
  aclk ps_0/FCLK_CLK0
  resetn proc_sys_reset_0/peripheral_aresetn
  s_axis axis_clock_converter_0/M_AXIS
}

# DMA

cell xilinx.com:ip:axi_dma:7.1 axi_dma_0 {
  c_include_sg 1
  c_sg_include_stscntrl_strm 0
  c_sg_length_width 20
  c_s2mm_burst_size 16
  c_m_axi_s2mm_data_width 64
  c_m_axi_mm2s_data_width 64
  c_m_axis_mm2s_tdata_width 64
  c_mm2s_burst_size 16
} {
  S_AXI_LITE axi_mem_intercon_0/M[add_master_interface]_AXI
  s_axi_lite_aclk ps_0/FCLK_CLK0
  M_AXI_SG dma_interconnect/S00_AXI
  m_axi_sg_aclk ps_0/FCLK_CLK0
  M_AXI_MM2S dma_interconnect/S01_AXI
  m_axi_mm2s_aclk ps_0/FCLK_CLK0
  M_AXI_S2MM dma_interconnect/S02_AXI
  m_axi_s2mm_aclk ps_0/FCLK_CLK0
  S_AXIS_S2MM tlast_gen_0/m_axis
  axi_resetn proc_sys_reset_0/peripheral_aresetn
}

# DAC Streaming (MM2S)

cell xilinx.com:ip:axis_clock_converter:1.1 axis_clock_converter_1 {
  TDATA_NUM_BYTES 8
} {
  S_AXIS axi_dma_0/M_AXIS_MM2S
  s_axis_aclk ps_0/FCLK_CLK0
  s_axis_aresetn proc_sys_reset_0/peripheral_aresetn
  m_axis_aclk adc_dac/adc_clk
  m_axis_aresetn rst_adc_clk/peripheral_aresetn
}

cell xilinx.com:ip:axis_dwidth_converter:1.1 axis_dwidth_converter_1 {
  S_TDATA_NUM_BYTES 8
  M_TDATA_NUM_BYTES 2
} {
  aclk adc_dac/adc_clk
  aresetn rst_adc_clk/peripheral_aresetn
  S_AXIS axis_clock_converter_1/M_AXIS
  m_axis_tvalid axis_dwidth_converter_1/m_axis_tready
}

connect_pins adc_dac/dac0 axis_dwidth_converter_1/m_axis_tdata
connect_pins adc_dac/dac1 axis_dwidth_converter_1/m_axis_tdata

# DMA AXI Lite
assign_bd_address [get_bd_addr_segs {axi_dma_0/S_AXI_LITE/Reg }]
set_property range [get_memory_range dma] [get_bd_addr_segs {ps_0/Data/SEG_axi_dma_0_Reg}]
set_property offset [get_memory_offset dma] [get_bd_addr_segs {ps_0/Data/SEG_axi_dma_0_Reg}]

# Scatter Gather interface in On Chip Memory
assign_bd_address [get_bd_addr_segs {ps_0/S_AXI_GP0/GP0_HIGH_OCM }]
set_property range 64K [get_bd_addr_segs {axi_dma_0/Data_SG/SEG_ps_0_GP0_HIGH_OCM}]
set_property offset [get_memory_offset ocm_mm2s] [get_bd_addr_segs {axi_dma_0/Data_SG/SEG_ps_0_GP0_HIGH_OCM}]

# MM2S on HP2
assign_bd_address [get_bd_addr_segs {ps_0/S_AXI_HP0/HP0_DDR_LOWOCM }]
set_property range [get_memory_range ram_mm2s] [get_bd_addr_segs {axi_dma_0/Data_MM2S/SEG_ps_0_HP0_DDR_LOWOCM}]
set_property offset [get_memory_offset ram_mm2s] [get_bd_addr_segs {axi_dma_0/Data_MM2S/SEG_ps_0_HP0_DDR_LOWOCM}]

# S2MM on HP0
assign_bd_address [get_bd_addr_segs {ps_0/S_AXI_HP2/HP2_DDR_LOWOCM }]
set_property range [get_memory_range ram_s2mm] [get_bd_addr_segs {axi_dma_0/Data_S2MM/SEG_ps_0_HP2_DDR_LOWOCM}]
set_property offset [get_memory_offset ram_s2mm] [get_bd_addr_segs {axi_dma_0/Data_S2MM/SEG_ps_0_HP2_DDR_LOWOCM}]

# Unmap unused segments
delete_bd_objs [get_bd_addr_segs axi_dma_0/Data_SG/SEG_ps_0_HP0_DDR_LOWOCM]
delete_bd_objs [get_bd_addr_segs axi_dma_0/Data_SG/SEG_ps_0_HP2_DDR_LOWOCM]
delete_bd_objs [get_bd_addr_segs axi_dma_0/Data_MM2S/SEG_ps_0_GP0_HIGH_OCM]
delete_bd_objs [get_bd_addr_segs axi_dma_0/Data_MM2S/SEG_ps_0_HP2_DDR_LOWOCM]
delete_bd_objs [get_bd_addr_segs axi_dma_0/Data_S2MM/SEG_ps_0_GP0_HIGH_OCM]
delete_bd_objs [get_bd_addr_segs axi_dma_0/Data_S2MM/SEG_ps_0_HP0_DDR_LOWOCM]

# Hack to change the 32 bit auto width in AXI_DMA S_AXI_S2MM
validate_bd_design
