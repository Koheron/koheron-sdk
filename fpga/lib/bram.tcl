
proc add_bram {memory_name {intercon_idx 0} {default_hexval 0}} {

  # Add a new Master Interface to AXI Interconnect
  set idx [add_master_interface $intercon_idx]

  # Add BRAM Controller
  cell xilinx.com:ip:axi_bram_ctrl:4.0 axi_bram_ctrl_$memory_name {
    SINGLE_PORT_BRAM 1
    PROTOCOL AXI4LITE
  } {
    s_axi_aclk /[set ::ps_clk$intercon_idx]
    s_axi_aresetn /[set ::rst${intercon_idx}_name]/peripheral_aresetn
    S_AXI /axi_mem_intercon_$intercon_idx/M${idx}_AXI
  }

  set bram_name blk_mem_gen_$memory_name

  # Add Block Memory Generator
  cell xilinx.com:ip:blk_mem_gen:8.3 $bram_name {
    use_bram_block Stand_Alone
    Enable_32bit_Address true
    use_RSTB_Pin true
    Output_Reset_Value_B $default_hexval
    Memory_Type True_Dual_Port_RAM
    Fill_Remaining_Memory_Locations true
    Remaining_Memory_Locations $default_hexval
    Write_Depth_A [get_memory_depth $memory_name]
  } {
    BRAM_PORTA axi_bram_ctrl_$memory_name/BRAM_PORTA
  }

  assign_bd_address   [get_bd_addr_segs {axi_bram_ctrl_$memory_name/S_AXI/Mem0 }]
  set memory_segment  [get_bd_addr_segs /${::ps_name}/Data/SEG_axi_bram_ctrl_${memory_name}_Mem0]
  set_property offset [get_memory_offset $memory_name] $memory_segment
  set_property range  [get_memory_range $memory_name]  $memory_segment

  # A register slice on the master side is helpful since the net delays to the BRAMs can be quite long
  set_property -dict [list CONFIG.M${idx}_HAS_REGSLICE 1] [get_bd_cells /axi_mem_intercon_$intercon_idx]

  return $bram_name
}
