proc add_bram {bram_name bram_size} {
  # Add a new Master Interface to AXI Interconnect
  set num_master_interfaces [get_property CONFIG.NUM_MI [get_bd_cells ${::ps_name}_axi_periph]]
  incr num_master_interfaces
  properties ${::ps_name}_axi_periph [list NUM_MI $num_master_interfaces]
  # Add BRAM
  create_bd_cell -type ip -vlnv xilinx.com:ip:axi_bram_ctrl:4.0 axi_bram_ctrl_$bram_name
  create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_$bram_name
  properties blk_mem_gen_$bram_name {Memory_Type True_Dual_Port_RAM}
  apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config [list Master "/${::ps_name}/M_AXI_GP0" Clk "Auto"] [get_bd_intf_pins axi_bram_ctrl_$bram_name/S_AXI]
  connect_bd_intf_net [get_bd_intf_pins axi_bram_ctrl_$bram_name/BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_$bram_name/BRAM_PORTA]
  properties axi_bram_ctrl_$bram_name {SINGLE_PORT_BRAM 1}
  set_property range $bram_size [get_bd_addr_segs ${::ps_name}/Data/SEG_axi_bram_ctrl_${bram_name}_Mem0]
}
