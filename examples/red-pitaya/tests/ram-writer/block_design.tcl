# Add PS and AXI Interconnect
set board_preset $board_path/config/board_preset.tcl
source $sdk_path/fpga/lib/starting_point.tcl

source $sdk_path/fpga/lib/ctl_sts.tcl
add_ctl_sts ps_0/FCLK_CLK0 proc_sys_reset_0/peripheral_aresetn

set_property -dict [list \
  CONFIG.PCW_USE_DEFAULT_ACP_USER_VAL {1} \
  CONFIG.PCW_USE_S_AXI_ACP {1} \
] [get_bd_cells ps_0]

cell xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0 {
  Output_Width 64
} {
  CLK ps_0/FCLK_CLK1
}

cell xilinx.com:ip:axis_clock_converter:1.1 axis_clock_converter_0 {
  TDATA_NUM_BYTES 8
} {
  s_axis_tdata c_counter_binary_0/Q
  s_axis_tvalid [get_constant_pin 1 1]
  s_axis_aclk ps_0/FCLK_CLK1
  m_axis_aclk ps_0/FCLK_CLK0
  m_axis_aresetn proc_sys_reset_0/peripheral_aresetn
  s_axis_aresetn proc_sys_reset_0/peripheral_aresetn
}

cell pavel-demin:user:axis_ram_writer:1.0 writer_0 {
  ADDR_WIDTH 16
  AXI_ID_WIDTH 3
  AXIS_TDATA_WIDTH 64
  FIFO_WRITE_DEPTH 512
} {
  s_axis axis_clock_converter_0/M_AXIS
  aclk ps_0/FCLK_CLK0
  aresetn proc_sys_reset_0/peripheral_aresetn
  min_addr [ctl_pin min_addr]
  cfg_data [ctl_pin cfg_data]
  sts_data [sts_pin sts_data]
  m_axi ps_0/S_AXI_ACP
}

apply_bd_automation -rule xilinx.com:bd_rule:clkrst -config { Clk {/ps_0/FCLK_CLK0 (100 MHz)} Freq {100} Ref_Clk0 {} Ref_Clk1 {} Ref_Clk2 {}}  [get_bd_pins ps_0/S_AXI_ACP_ACLK]

assign_bd_address -target_address_space /writer_0/m_axi [get_bd_addr_segs ps_0/S_AXI_ACP/ACP_DDR_LOWOCM] -force
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_ACP/ACP_QSPI_LINEAR] -target_address_space [get_bd_addr_spaces writer_0/m_axi]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_ACP/ACP_M_AXI_GP0] -target_address_space [get_bd_addr_spaces writer_0/m_axi]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_ACP/ACP_IOP] -target_address_space [get_bd_addr_spaces writer_0/m_axi]
