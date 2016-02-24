proc add_bram {bram_name bram_size} {
  # Add a new Master Interface to AXI Interconnect
  set num_mi [get_property CONFIG.NUM_MI [get_bd_cells axi_mem_intercon]]
  if { $num_mi < 10 } {
    set idx 0$num_mi
  } else {
    set idx $num_mi
  }
  incr num_mi
  properties axi_mem_intercon [list NUM_MI $num_mi]
  connect_pins /axi_mem_intercon/M${idx}_ACLK    /${::ps_name}/FCLK_CLK0
  connect_pins /axi_mem_intercon/M${idx}_ARESETN /${::rst_name}/peripheral_aresetn

  # Add BRAM
  cell xilinx.com:ip:axi_bram_ctrl:4.0 axi_bram_ctrl_$bram_name {
    SINGLE_PORT_BRAM 1
  } {
    s_axi_aclk ${::ps_name}/FCLK_CLK0
    s_axi_aresetn ${::rst_name}/peripheral_aresetn
  }
  cell xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_$bram_name {
    Memory_Type True_Dual_Port_RAM
  } {}
  connect_bd_intf_net [get_bd_intf_pins axi_bram_ctrl_$bram_name/BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_$bram_name/BRAM_PORTA]
  set_property range $bram_size [get_bd_addr_segs ${::ps_name}/Data/SEG_axi_bram_ctrl_${bram_name}_Mem0]
}
