# Shared single-channel CIC/FIR, metadata FIFO and cyclic SG DMA.
cell koheron:user:latched_mux:1.0 phase_mux {
    WIDTH [get_parameter phase_accumulator_width]
    N_INPUTS 2
    SEL_WIDTH 1
} {
    clk adc_dac/adc_clk
    clken [get_constant_pin 1 1]
    din [get_concat_pin [list cordic0/phase cordic1/phase]]
    sel [get_slice_pin [ctl_pin cordic] 4 4]
}

cell koheron:user:latched_mux:1.0 overflow_mux {
    WIDTH 1
    N_INPUTS 2
    SEL_WIDTH 1
} {
    clk adc_dac/adc_clk
    clken [get_constant_pin 1 1]
    din [get_concat_pin [list cordic0/overflow cordic1/overflow]]
    sel [get_slice_pin [ctl_pin cordic] 4 4]
}

cell koheron:user:phase_range_guard:1.0 phase_range {
  INPUT_WIDTH [get_parameter phase_accumulator_width]
  OUTPUT_WIDTH 32
} {
  clk adc_dac/adc_clk
  aresetn phase_stream_control/filter_resetn
  din phase_mux/dout
}
cell xilinx.com:ip:util_vector_logic:2.0 phase_overflow {
  C_SIZE 1 C_OPERATION or
} {
  Op1 overflow_mux/dout
  Op2 phase_range/overflow
  Res phase_stream_control/upstream_overflow
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
  Input_Sample_Frequency [expr [get_parameter adc_clk] / 1000000.0]
  Clock_Frequency [expr [get_parameter adc_clk] / 1000000.0]
  Input_Data_Width 32
  Quantization Truncation
  Output_Data_Width [get_parameter phase_filter_width]
  Use_Xtreme_DSP_Slice false
  HAS_DOUT_TREADY true
  HAS_ARESETN true
} {
  aclk adc_dac/adc_clk
  s_axis_data_tdata phase_range/dout
  s_axis_data_tvalid phase_stream_control/data_valid
  s_axis_config_tvalid phase_stream_control/config_valid
  s_axis_config_tdata phase_stream_control/config_rate
  aresetn phase_stream_control/filter_resetn
}

set fir_coeffs [exec -- env -i $python -I fpga/scripts/fir.py $n_stages $dec_rate_min $diff_delay print]

cell xilinx.com:ip:fir_compiler:7.2 fir {
  Filter_Type Decimation
  Sample_Frequency [expr [get_parameter adc_clk] / 1000000. / $dec_rate_min]
  Clock_Frequency [expr [get_parameter adc_clk] / 1000000.]
  Coefficient_Width 32
  Data_Width [get_parameter phase_filter_width]
  Output_Rounding_Mode Convergent_Rounding_to_Even
  Output_Width [get_parameter phase_filter_width]
  Decimation_Rate 2
  BestPrecision true
  CoefficientVector [subst {{$fir_coeffs}}]
  M_DATA_Has_TREADY true
  Has_ARESETn true
  Reset_Data_Vector true
} {
  aclk adc_dac/adc_clk
  S_AXIS_DATA cic/M_AXIS_DATA
  aresetn phase_stream_control/filter_resetn
}

# Control registers and DDR traffic use independent interconnects. DMA cannot
# address register slaves; the smaller 2-to-1 DDR fabric fits Zynq-7010 too.
set_property -dict [list CONFIG.PCW_USE_S_AXI_HP0 {1} CONFIG.PCW_S_AXI_HP0_DATA_WIDTH {32}] [get_bd_cells ps_0]
connect_pins ps_0/S_AXI_HP0_ACLK ps_0/FCLK_CLK1
set_property CONFIG.NUM_MI 2 [get_bd_cells axi_mem_intercon_1]
cell xilinx.com:ip:axi_interconnect:2.1 phase_dma_memory {
  NUM_SI 2
  NUM_MI 1
  STRATEGY 1
} {
  ACLK ps_0/FCLK_CLK1
  ARESETN proc_sys_reset_1/interconnect_aresetn
  S00_ACLK ps_0/FCLK_CLK1
  S01_ACLK ps_0/FCLK_CLK1
  M00_ACLK ps_0/FCLK_CLK1
  S00_ARESETN proc_sys_reset_1/peripheral_aresetn
  S01_ARESETN proc_sys_reset_1/peripheral_aresetn
  M00_ARESETN proc_sys_reset_1/peripheral_aresetn
  M00_AXI ps_0/S_AXI_HP0
}

cell koheron:user:phase_quantizer:1.0 phase_quantizer {
  PKT_LENGTH 8192
  BASE_SHIFT 8
} {
  aclk adc_dac/adc_clk
  aresetn phase_stream_control/filter_resetn
  requested_bits phase_stream_control/active_bits
  upstream_overflow phase_stream_control/overflow
  S_AXIS fir/M_AXIS_DATA
  packet_status [sts_pin phase_packet]
}

cell xilinx.com:ip:xlconcat:2.1 sample_metadata { NUM_PORTS 2 } {
  In0 phase_quantizer/sample_status
  In1 phase_stream_control/sample_gap
}
cell koheron:user:axis_stream_packet_mux:1.0 phase_packet_framer {} {
  S_AXI_LITE axi_mem_intercon_1/M00_AXI
  aclk ps_0/FCLK_CLK1
  aresetn proc_sys_reset_1/peripheral_aresetn
  s_axis_1_tdata [get_constant_pin 0 32]
  s_axis_1_tuser [get_constant_pin 0 6]
  s_axis_1_tvalid [get_constant_pin 0 1]
}
cell xilinx.com:ip:axis_data_fifo:2.0 phase_fifo {
  FIFO_DEPTH 16384
  TDATA_NUM_BYTES 4
  TUSER_WIDTH 6
  IS_ACLK_ASYNC 1
} {
  S_AXIS phase_quantizer/M_AXIS
  s_axis_tuser sample_metadata/dout
  s_axis_aclk adc_dac/adc_clk
  m_axis_aclk ps_0/FCLK_CLK1
  s_axis_aresetn phase_stream_control/filter_resetn
  M_AXIS phase_packet_framer/S_AXIS_0
}
cell xilinx.com:ip:axi_dma:7.1 axi_dma_0 {
  c_include_sg 1
  c_include_mm2s 0
  c_sg_include_stscntrl_strm 0
  c_sg_length_width 23
  c_s2mm_burst_size 16
} {
  S_AXIS_S2MM phase_packet_framer/M_AXIS
  S_AXI_LITE axi_mem_intercon_1/M01_AXI
  s_axi_lite_aclk ps_0/FCLK_CLK1
  M_AXI_S2MM phase_dma_memory/S00_AXI
  M_AXI_SG phase_dma_memory/S01_AXI
  m_axi_sg_aclk ps_0/FCLK_CLK1
  m_axi_s2mm_aclk ps_0/FCLK_CLK1
  axi_resetn proc_sys_reset_1/peripheral_aresetn
  s2mm_introut [get_interrupt_pin]
}

connect_bd_net [get_bd_pins axi_mem_intercon_1/M01_ACLK] [get_bd_pins ps_0/FCLK_CLK1]
connect_bd_net [get_bd_pins axi_mem_intercon_1/M01_ARESETN] [get_bd_pins proc_sys_reset_1/peripheral_aresetn]

assign_bd_address [get_bd_addr_segs {axi_dma_0/S_AXI_LITE/Reg }]
set_property range [get_memory_range dma] [get_bd_addr_segs {ps_0/Data/SEG_axi_dma_0_Reg}]
set_property offset [get_memory_offset dma] [get_bd_addr_segs {ps_0/Data/SEG_axi_dma_0_Reg}]

assign_bd_address [get_bd_addr_segs {ps_0/S_AXI_HP0/HP0_DDR_LOWOCM }]
set_property range [get_memory_range ram] [get_bd_addr_segs {axi_dma_0/Data_S2MM/SEG_ps_0_HP0_DDR_LOWOCM}]
set_property offset [get_memory_offset ram] [get_bd_addr_segs {axi_dma_0/Data_S2MM/SEG_ps_0_HP0_DDR_LOWOCM}]

connect_pins axi_mem_intercon_1/M00_ACLK ps_0/FCLK_CLK1
connect_pins axi_mem_intercon_1/M00_ARESETN proc_sys_reset_1/peripheral_aresetn
set_property range [get_memory_range ram] [get_bd_addr_segs {axi_dma_0/Data_SG/SEG_ps_0_HP0_DDR_LOWOCM}]
set_property offset [get_memory_offset ram] [get_bd_addr_segs {axi_dma_0/Data_SG/SEG_ps_0_HP0_DDR_LOWOCM}]
assign_bd_address -target_address_space /ps_0/Data [get_bd_addr_segs phase_packet_framer/s_axi/reg0] -force
set_property range [get_memory_range mux] [get_bd_addr_segs {ps_0/Data/SEG_phase_packet_framer_reg0}]
set_property offset [get_memory_offset mux] [get_bd_addr_segs {ps_0/Data/SEG_phase_packet_framer_reg0}]

connect_pins phase_stream_control/data_ready cic/s_axis_data_tready
connect_pins phase_stream_control/config_ready cic/s_axis_config_tready
