
proc add_bram {bram_name bram_range {bram_offset "auto"} {idx "auto"} {intercon_idx 1}} {

  if { $idx eq "auto"} {
    # Add a new Master Interface to AXI Interconnect
    set idx [add_master_interface $intercon_idx]
  }

  # Add BRAM Controller
  cell xilinx.com:ip:axi_bram_ctrl:4.0 axi_bram_ctrl_$bram_name {
    SINGLE_PORT_BRAM 1
  } {
    s_axi_aclk ${::ps_name}/FCLK_CLK0
    s_axi_aresetn ${::rst_name}/peripheral_aresetn
  }
  connect_bd_intf_net [get_bd_intf_pins axi_bram_ctrl_$bram_name/S_AXI] [get_bd_intf_pins axi_mem_intercon_$intercon_idx/M${idx}_AXI]

  # Add Block Memory Generator
  cell xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_$bram_name {
    Memory_Type True_Dual_Port_RAM
  } {}
  connect_bd_intf_net [get_bd_intf_pins axi_bram_ctrl_$bram_name/BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_$bram_name/BRAM_PORTA]

  assign_bd_address [get_bd_addr_segs {axi_bram_ctrl_$bram_name/S_AXI/Mem0 }]
  set memory_segment [get_bd_addr_segs ${::ps_name}/Data/SEG_axi_bram_ctrl_${bram_name}_Mem0]
  if { $bram_offset ne "auto"} {
    set_property offset $bram_offset $memory_segment
  }
  set_property range $bram_range $memory_segment

  # Use data fifo (depth 32) to help timing closure
  #set_property -dict [list CONFIG.M${idx}_HAS_DATA_FIFO 1] [get_bd_cells axi_mem_intercon]
  set_property -dict [list CONFIG.M${idx}_HAS_REGSLICE 1] [get_bd_cells axi_mem_intercon_$intercon_idx]
}
