set board_preset $board_path/config/board_preset.tcl
source $sdk_path/fpga/lib/starting_point.tcl
set adc_dac_extra_delay 2
source $board_path/adc.tcl
source $sdk_path/fpga/lib/ctl_sts.tcl
add_ctl_sts adc/adc_clk rst_adc_clk/peripheral_aresetn
connect_cell adc {
  ctl [ctl_pin mmcm]
  cfg_data [ps_ctl_pin spi_cfg_data]
  cfg_cmd [ps_ctl_pin spi_cfg_cmd]
  cfg_sts [ps_sts_pin spi_cfg_sts]
}
connect_pins [sts_pin pll_locked] [get_concat_pin [list adc/pll_locked [get_constant_pin 0 31]]]

create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_analog_io_rtl:1.0 Vp_Vn
cell xilinx.com:ip:xlconcat:2.1 concat_interrupts {
  NUM_PORTS 1
} {
  dout ps_0/IRQ_F2P
}
cell xilinx.com:ip:xadc_wiz:3.3 xadc_wiz_0 {} {
  Vp_Vn Vp_Vn
  s_axi_lite axi_mem_intercon_0/M[add_master_interface]_AXI
  s_axi_aclk ps_0/FCLK_CLK0
  s_axi_aresetn proc_sys_reset_0/peripheral_aresetn
  ip2intc_irpt concat_interrupts/In0
}
assign_bd_address [get_bd_addr_segs xadc_wiz_0/s_axi_lite/Reg]
set_property offset [get_memory_offset xadc] [get_bd_addr_segs ps_0/Data/SEG_xadc_wiz_0_Reg]
for {set i 0} {$i < 8} {incr i} {
  create_bd_port -dir I exp_io_${i}_p
  create_bd_port -dir O exp_io_${i}_n
}
source $board_path/spi.tcl
connect_pins ps_0/SDIO0_CDN [get_constant_pin 0 1]
connect_pins ps_0/SDIO0_WP [get_constant_pin 0 1]

# HP0 and HP2 use the two separate PL-facing DDR controller paths.
set_cell_props ps_0 {
  PCW_USE_S_AXI_HP0 1
  PCW_S_AXI_HP0_DATA_WIDTH 64
  PCW_USE_S_AXI_HP2 1
  PCW_S_AXI_HP2_DATA_WIDTH 64
  PCW_USE_S_AXI_GP0 1
  PCW_USE_HIGH_OCM 1
}
connect_pins ps_0/S_AXI_GP0_ACLK ps_0/FCLK_CLK0
connect_pins ps_0/S_AXI_HP0_ACLK ps_0/FCLK_CLK0
connect_pins ps_0/S_AXI_HP2_ACLK ps_0/FCLK_CLK0

# Descriptor traffic uses OCM instead of competing for DDR bandwidth.
cell xilinx.com:ip:axi_interconnect:2.1 sg_interconnect {
  NUM_SI 2
  NUM_MI 1
} {
  ACLK ps_0/FCLK_CLK0
  ARESETN proc_sys_reset_0/peripheral_aresetn
  S00_ACLK ps_0/FCLK_CLK0
  S01_ACLK ps_0/FCLK_CLK0
  M00_ACLK ps_0/FCLK_CLK0
  S00_ARESETN proc_sys_reset_0/peripheral_aresetn
  S01_ARESETN proc_sys_reset_0/peripheral_aresetn
  M00_ARESETN proc_sys_reset_0/peripheral_aresetn
  M00_AXI ps_0/S_AXI_GP0
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
cell koheron:user:quad_capture:1.0 capture {} {
  clk adc/adc_clk
  aresetn stream_reset/Res
  trigger [get_slice_pin [ctl_pin trigger] 0 0]
  sample_count [ctl_pin sample_count]
  test_pattern [get_slice_pin [ctl_pin test_pattern] 0 0]
  adc00 adc/adc00
  adc01 adc/adc01
  adc10 adc/adc10
  adc11 adc/adc11
  status [sts_pin capture_status]
  captured_samples [sts_pin captured_samples]
}

for {set i 0} {$i < 2} {incr i} {
  set hp [expr $i * 2]
  cell xilinx.com:ip:axis_data_fifo:2.0 fifo$i {
    FIFO_DEPTH 8192
    FIFO_MEMORY_TYPE block
  } {
    s_axis_aclk adc/adc_clk
    s_axis_aresetn stream_reset/Res
    S_AXIS capture/m${i}_axis
  }
  cell xilinx.com:ip:axis_clock_converter:1.1 cdc$i {
    TDATA_NUM_BYTES 8
  } {
    s_axis_aclk adc/adc_clk
    s_axis_aresetn stream_reset/Res
    m_axis_aclk ps_0/FCLK_CLK0
    S_AXIS fifo$i/M_AXIS
  }
  cell xilinx.com:ip:axi_interconnect:2.1 data_interconnect$i {
    NUM_SI 1
    NUM_MI 1
    S00_HAS_REGSLICE 1
    S00_HAS_DATA_FIFO 2
  } {
    ACLK ps_0/FCLK_CLK0
    ARESETN proc_sys_reset_0/peripheral_aresetn
    S00_ACLK ps_0/FCLK_CLK0
    M00_ACLK ps_0/FCLK_CLK0
    S00_ARESETN proc_sys_reset_0/peripheral_aresetn
    M00_ARESETN proc_sys_reset_0/peripheral_aresetn
    M00_AXI ps_0/S_AXI_HP$hp
  }
  cell xilinx.com:ip:axi_dma:7.1 dma$i {
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
    M_AXI_SG sg_interconnect/S0${i}_AXI
    m_axi_sg_aclk ps_0/FCLK_CLK0
    M_AXI_S2MM data_interconnect$i/S00_AXI
    m_axi_s2mm_aclk ps_0/FCLK_CLK0
    S_AXIS_S2MM cdc$i/M_AXIS
    axi_resetn proc_sys_reset_0/peripheral_aresetn
    s2mm_introut [get_interrupt_pin]
  }
  connect_pins cdc$i/m_axis_aresetn dma$i/s2mm_prmry_reset_out_n
  assign_bd_address [get_bd_addr_segs dma$i/S_AXI_LITE/Reg]
  set_property range [get_memory_range dma$i] [get_bd_addr_segs ps_0/Data/SEG_dma${i}_Reg]
  set_property offset [get_memory_offset dma$i] [get_bd_addr_segs ps_0/Data/SEG_dma${i}_Reg]

  assign_bd_address -offset 0xFFFF0000 -range 64K -force \
    -target_address_space [get_bd_addr_spaces dma$i/Data_SG] \
    [get_bd_addr_segs ps_0/S_AXI_GP0/GP0_HIGH_OCM]

  assign_bd_address -target_address_space [get_bd_addr_spaces dma$i/Data_S2MM] [get_bd_addr_segs ps_0/S_AXI_HP$hp/HP${hp}_DDR_LOWOCM]
  set_property range [get_memory_range ram$i] [get_bd_addr_segs dma$i/Data_S2MM/SEG_ps_0_HP${hp}_DDR_LOWOCM]
  set_property offset [get_memory_offset ram$i] [get_bd_addr_segs dma$i/Data_S2MM/SEG_ps_0_HP${hp}_DDR_LOWOCM]
}
validate_bd_design
