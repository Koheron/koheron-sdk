source $board_path/starting_point.tcl

# Receive-only DMA: all 128 MiB belong to ADC0. Descriptors live in OCM.
set_cell_props ps_0 {
  PCW_USE_S_AXI_HP0 1
  PCW_S_AXI_HP0_DATA_WIDTH 64
  PCW_USE_S_AXI_GP0 1
  PCW_USE_HIGH_OCM 1
}
connect_pins ps_0/S_AXI_GP0_ACLK ps_0/FCLK_CLK0
connect_pins ps_0/S_AXI_HP0_ACLK ps_0/FCLK_CLK0

cell xilinx.com:ip:axi_interconnect:2.1 dma_interconnect {
  NUM_SI 2
  NUM_MI 2
  S01_HAS_REGSLICE 1
} {
  ACLK ps_0/FCLK_CLK0
  ARESETN proc_sys_reset_0/peripheral_aresetn
  M00_AXI ps_0/S_AXI_GP0
  M01_AXI ps_0/S_AXI_HP0
  S00_ACLK ps_0/FCLK_CLK0
  S00_ARESETN proc_sys_reset_0/peripheral_aresetn
  S01_ACLK ps_0/FCLK_CLK0
  S01_ARESETN proc_sys_reset_0/peripheral_aresetn
  M00_ACLK ps_0/FCLK_CLK0
  M00_ARESETN proc_sys_reset_0/peripheral_aresetn
  M01_ACLK ps_0/FCLK_CLK0
  M01_ARESETN proc_sys_reset_0/peripheral_aresetn
}

cell xilinx.com:ip:util_vector_logic:2.0 reset_not {
  C_OPERATION not
  C_SIZE 1
} {
  Op1 [get_slice_pin [ctl_pin acq_reset] 0 0]
}
cell xilinx.com:ip:util_vector_logic:2.0 stream_reset {
  C_OPERATION and
  C_SIZE 1
} {
  Op1 reset_not/Res
  Op2 rst_adc_clk/peripheral_aresetn
}

cell koheron:user:chirp_generator:1.0 chirp {
} {
  clk adc_dac/adc_clk
  aresetn rst_adc_clk/peripheral_aresetn
  reset [get_slice_pin [ctl_pin acq_reset] 0 0]
  trigger [get_slice_pin [ctl_pin trigger] 0 0]
  sample_count [ctl_pin chirp_samples]
  coefficient [get_concat_pin [list [ctl_pin chirp_coefficient0] [get_slice_pin [ctl_pin chirp_coefficient1] 15 0]]]
  seed_data [get_concat_pin [list [ctl_pin seed_data0] [ctl_pin seed_data1]]]
  seed_index [get_slice_pin [ctl_pin seed_index] 3 0]
  seed_write [get_slice_pin [ctl_pin seed_write] 0 0]
}

# Phase accumulation is in chirp; the DDS only converts phase into a sine.
cell xilinx.com:ip:dds_compiler:6.0 sine_lut {
  PartsPresent SIN_COS_LUT_only
  Output_Selection Sine
  Parameter_Entry Hardware_Parameters
  Phase_Width 16
  Output_Width 16
  Noise_Shaping None
  Has_Phase_Out false
  Has_ARESETn true
  S_PHASE_Has_TUSER User_Field
  S_PHASE_TUSER_Width 1
  M_DATA_Has_TUSER User_Field
  Latency_Configuration Configurable
  Latency 8
} {
  aclk adc_dac/adc_clk
  aresetn stream_reset/Res
  s_axis_phase_tdata [get_slice_pin chirp/phase 47 32]
  s_axis_phase_tvalid [get_constant_pin 1 1]
  s_axis_phase_tuser chirp/phase_valid
}

# Keep the LUT clocking after the last chirp sample to drain its pipeline.
# TUSER carries the excitation-valid tag alongside the corresponding sine.
cell xilinx.com:ip:util_vector_logic:2.0 sine_valid {
  C_OPERATION and
  C_SIZE 1
} {
  Op1 sine_lut/m_axis_data_tvalid
  Op2 sine_lut/m_axis_data_tuser
}

cell koheron:user:adc_capture:1.0 capture {
} {
  clk adc_dac/adc_clk
  aresetn stream_reset/Res
  adc adc_dac/adc0
  sine sine_lut/m_axis_data_tdata
  sine_valid sine_valid/Res
  sample_count [ctl_pin chirp_samples]
  status [sts_pin capture_status]
  dac adc_dac/dac0
}
connect_pins adc_dac/dac1 [get_constant_pin 0 16]

# Buffer short DDR stalls; sticky overflow rejects a non-contiguous capture.
cell xilinx.com:ip:axis_data_fifo:2.0 capture_fifo {
  FIFO_DEPTH 4096
  FIFO_MEMORY_TYPE block
} {
  s_axis_aclk adc_dac/adc_clk
  s_axis_aresetn stream_reset/Res
  S_AXIS capture/M_AXIS
}
cell xilinx.com:ip:axis_clock_converter:1.1 capture_cdc {
  TDATA_NUM_BYTES 8
} {
  s_axis_aclk adc_dac/adc_clk
  s_axis_aresetn stream_reset/Res
  m_axis_aclk ps_0/FCLK_CLK0
  S_AXIS capture_fifo/M_AXIS
}

cell xilinx.com:ip:axi_dma:7.1 axi_dma_0 {
  c_include_mm2s 0
  c_include_sg 1
  c_sg_include_stscntrl_strm 0
  c_sg_length_width 20
  c_s2mm_burst_size 16
  c_m_axi_s2mm_data_width 64
  c_s_axis_s2mm_tdata_width 64
} {
  S_AXI_LITE axi_mem_intercon_0/M[add_master_interface]_AXI
  s_axi_lite_aclk ps_0/FCLK_CLK0
  M_AXI_SG dma_interconnect/S00_AXI
  m_axi_sg_aclk ps_0/FCLK_CLK0
  M_AXI_S2MM dma_interconnect/S01_AXI
  m_axi_s2mm_aclk ps_0/FCLK_CLK0
  S_AXIS_S2MM capture_cdc/M_AXIS
  axi_resetn proc_sys_reset_0/peripheral_aresetn
  s2mm_introut [get_interrupt_pin]
}
connect_pins capture_cdc/m_axis_aresetn axi_dma_0/s2mm_prmry_reset_out_n

assign_bd_address [get_bd_addr_segs axi_dma_0/S_AXI_LITE/Reg]
set_property range [get_memory_range dma] [get_bd_addr_segs ps_0/Data/SEG_axi_dma_0_Reg]
set_property offset [get_memory_offset dma] [get_bd_addr_segs ps_0/Data/SEG_axi_dma_0_Reg]

assign_bd_address [get_bd_addr_segs ps_0/S_AXI_GP0/GP0_HIGH_OCM]
set_property range 64K [get_bd_addr_segs axi_dma_0/Data_SG/SEG_ps_0_GP0_HIGH_OCM]
set_property offset [get_memory_offset ocm_s2mm] [get_bd_addr_segs axi_dma_0/Data_SG/SEG_ps_0_GP0_HIGH_OCM]
exclude_bd_addr_seg [get_bd_addr_segs axi_dma_0/Data_S2MM/SEG_ps_0_GP0_HIGH_OCM]

assign_bd_address [get_bd_addr_segs ps_0/S_AXI_HP0/HP0_DDR_LOWOCM]
set_property range [get_memory_range ram_s2mm] [get_bd_addr_segs axi_dma_0/Data_S2MM/SEG_ps_0_HP0_DDR_LOWOCM]
set_property offset [get_memory_offset ram_s2mm] [get_bd_addr_segs axi_dma_0/Data_S2MM/SEG_ps_0_HP0_DDR_LOWOCM]
exclude_bd_addr_seg [get_bd_addr_segs axi_dma_0/Data_SG/SEG_ps_0_HP0_DDR_LOWOCM]
validate_bd_design
