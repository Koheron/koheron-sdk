
proc add_status_register {module_name clk {num_ports 32} {range 4K} {offset "auto"} {idx "auto"} {intercon_idx 0}}  {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  for {set i 0} {$i < $num_ports} {incr i} {
    create_bd_pin -dir I -from 31 -to 0 In$i
  }

  if { $idx eq "auto"} {
    # Add a new Master Interface to AXI Interconnect
    set idx [add_master_interface $intercon_idx]
  }
  
  # AXI clock converter
  cell xilinx.com:ip:axi_clock_converter:2.1 axi_clock_converter_0 {} {
    s_axi_aclk /[set ::ps_clk$intercon_idx]
    s_axi_aresetn /[set ::rst${intercon_idx}_name]/peripheral_aresetn
    m_axi_aclk /$clk
    m_axi_aresetn /${::rst_adc_clk_name}/peripheral_aresetn
  }
  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins /axi_mem_intercon_$intercon_idx/M${idx}_AXI] [get_bd_intf_pins axi_clock_converter_0/S_AXI]

  # Sts register
  cell pavel-demin:user:axi_sts_register:1.0 axi_sts_register_0 {
    STS_DATA_WIDTH [expr $num_ports*32]
  } {
    aclk axi_clock_converter_0/m_axi_aclk
    aresetn /${::rst_adc_clk_name}/peripheral_aresetn
  }
  connect_bd_intf_net [get_bd_intf_pins axi_sts_register_0/S_AXI] [get_bd_intf_pins axi_clock_converter_0/M_AXI]

  assign_bd_address [get_bd_addr_segs {axi_sts_register_0/s_axi/reg0 }]
  set memory_segment [get_bd_addr_segs /${::ps_name}/Data/SEG_axi_sts_register_0_reg0]
  set_property range $range $memory_segment

  if { $offset ne "auto"} {
    set_property offset $offset $memory_segment
  }

  cell xilinx.com:ip:xlconcat:2.1 concat_0 {
    NUM_PORTS $num_ports
  } {
    dout axi_sts_register_0/sts_data
  }

  for {set i 0} {$i < $num_ports} {incr i} {
    set_property -dict \
     [list CONFIG.IN${i}_WIDTH 32] \
     [get_bd_cells concat_0]
    connect_pins concat_0/In$i In$i
  }

  current_bd_instance $bd

}
