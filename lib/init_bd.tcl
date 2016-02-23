proc init_bd {board_preset bram_name bram_size} {

  # Create processing_system7
  cell xilinx.com:ip:processing_system7:5.5 ${::ps_name} \
    [list PCW_USE_S_AXI_HP0 0] \
    [list M_AXI_GP0_ACLK ${::ps_name}/FCLK_CLK0]

  source $board_preset

  # Create all required interconnections
  apply_bd_automation -rule xilinx.com:bd_rule:processing_system7 -config {
    make_external {FIXED_IO, DDR}
    Master Disable
    Slave Disable
  } [get_bd_cells ${::ps_name}]

  create_bd_cell -type ip -vlnv xilinx.com:ip:axi_bram_ctrl:4.0 axi_bram_ctrl_$bram_name
  create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_$bram_name

  properties blk_mem_gen_$bram_name {Memory_Type True_Dual_Port_RAM}

  apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config \
    [list Master "/${::ps_name}/M_AXI_GP0" Clk "Auto" ] \
    [get_bd_intf_pins axi_bram_ctrl_$bram_name/S_AXI]

  apply_bd_automation -rule xilinx.com:bd_rule:bram_cntlr -config \
    [list BRAM "/blk_mem_gen_${bram_name}" ] \
    [get_bd_intf_pins axi_bram_ctrl_$bram_name/BRAM_PORTA]

  properties axi_bram_ctrl_$bram_name {SINGLE_PORT_BRAM 1}
  set_property range $bram_size [get_bd_addr_segs ${::ps_name}/Data/SEG_axi_bram_ctrl_${bram_name}_Mem0]

  set_property -dict [list CONFIG.S00_HAS_REGSLICE 4] [get_bd_cells axi_mem_intercon]
}
