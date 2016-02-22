
proc add_status_register {module_name clk num_ports} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  for {set i 0} {$i < $num_ports} {incr i} {
    create_bd_pin -dir I -from 31 -to 0 In$i
  }

  # Number of Master interfaces
  set num_mi [get_property CONFIG.NUM_MI [get_bd_cells /axi_mem_intercon]]
  
  if { $num_mi < 10 } {
    set idx 0$num_mi
  } else {
    set idx $num_mi
  }
  
  incr num_mi
  set_property -dict [list CONFIG.NUM_MI $num_mi] [get_bd_cells /axi_mem_intercon]

  connect_bd_net [get_bd_pins /axi_mem_intercon/M${idx}_ACLK] [get_bd_pins /${::ps_name}/FCLK_CLK0]
  connect_bd_net [get_bd_pins /axi_mem_intercon/M${idx}_ARESETN] [get_bd_pins /${::rst_name}/peripheral_aresetn]
  # AXI clock converter
  create_bd_cell -type ip -vlnv xilinx.com:ip:axi_clock_converter:2.1 axi_clock_converter_0
  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins /axi_mem_intercon/M${idx}_AXI] [get_bd_intf_pins axi_clock_converter_0/S_AXI]
  connect_bd_net [get_bd_pins axi_clock_converter_0/s_axi_aclk] [get_bd_pins /${::ps_name}/FCLK_CLK0]
  connect_bd_net [get_bd_pins axi_clock_converter_0/s_axi_aresetn] [get_bd_pins /${::rst_name}/peripheral_aresetn]
  connect_bd_net [get_bd_pins axi_clock_converter_0/m_axi_aclk] [get_bd_pins /$clk]
  connect_bd_net [get_bd_pins axi_clock_converter_0/m_axi_aresetn] [get_bd_pins /${::rst_name}/peripheral_aresetn]
  # Cfg register
  create_bd_cell -type ip -vlnv pavel-demin:user:axi_sts_register:1.0 axi_sts_register_0
  set_property -dict [list CONFIG.STS_DATA_WIDTH [expr $num_ports*32]] [get_bd_cells axi_sts_register_0]
  connect_bd_intf_net [get_bd_intf_pins axi_sts_register_0/S_AXI] [get_bd_intf_pins axi_clock_converter_0/M_AXI]
  connect_bd_net [get_bd_pins axi_sts_register_0/aclk] [get_bd_pins axi_clock_converter_0/m_axi_aclk]
  connect_bd_net [get_bd_pins axi_sts_register_0/aresetn] [get_bd_pins /${::rst_name}/peripheral_aresetn]
  #connect_bd_net [get_bd_pins axi_sts_register_0/sts_data] [get_bd_pins sts]
  assign_bd_address [get_bd_addr_segs {axi_sts_register_0/s_axi/reg0 }]
  set_property range 4K [get_bd_addr_segs /${::ps_name}/Data/SEG_axi_sts_register_0_reg0]

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
