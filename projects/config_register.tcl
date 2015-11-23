
proc add_config_register {module_name clk} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir O -from 1023 -to 0 cfg

  # Number of Master interfaces
  set num_mi [get_property CONFIG.NUM_MI [get_bd_cells /${::ps_name}_axi_periph]]
  
  if { $num_mi < 10 } {
    set idx 0$num_mi
  } else {
    set idx $num_mi
  }
  
  incr num_mi
  set_property -dict [list CONFIG.NUM_MI $num_mi] [get_bd_cells /${::ps_name}_axi_periph]

  connect_bd_net [get_bd_pins /${::ps_name}_axi_periph/M${idx}_ACLK] [get_bd_pins /${::ps_name}/FCLK_CLK0]
  connect_bd_net [get_bd_pins /${::ps_name}_axi_periph/M${idx}_ARESETN] [get_bd_pins /rst_${::ps_name}_125M/peripheral_aresetn]
  # AXI clock converter
  create_bd_cell -type ip -vlnv xilinx.com:ip:axi_clock_converter:2.1 axi_clock_converter_0
  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins /${::ps_name}_axi_periph/M${idx}_AXI] [get_bd_intf_pins axi_clock_converter_0/S_AXI]
  connect_bd_net [get_bd_pins axi_clock_converter_0/s_axi_aclk] [get_bd_pins /${::ps_name}/FCLK_CLK0]
  connect_bd_net [get_bd_pins axi_clock_converter_0/s_axi_aresetn] [get_bd_pins /rst_${::ps_name}_125M/peripheral_aresetn]
  connect_bd_net [get_bd_pins axi_clock_converter_0/m_axi_aclk] [get_bd_pins /$clk]
  connect_bd_net [get_bd_pins axi_clock_converter_0/m_axi_aresetn] [get_bd_pins /rst_${::ps_name}_125M/peripheral_aresetn]
  # Cfg register
  create_bd_cell -type ip -vlnv pavel-demin:user:axi_cfg_register:1.0 axi_cfg_register_0
  connect_bd_intf_net [get_bd_intf_pins axi_cfg_register_0/S_AXI] [get_bd_intf_pins axi_clock_converter_0/M_AXI]
  connect_bd_net [get_bd_pins axi_cfg_register_0/aclk] [get_bd_pins axi_clock_converter_0/m_axi_aclk]
  connect_bd_net [get_bd_pins axi_cfg_register_0/aresetn] [get_bd_pins /rst_${::ps_name}_125M/peripheral_aresetn]
  connect_bd_net [get_bd_pins axi_cfg_register_0/cfg_data] [get_bd_pins cfg]
  assign_bd_address [get_bd_addr_segs {axi_cfg_register_0/s_axi/reg0 }]
  set_property range 4K [get_bd_addr_segs /${::ps_name}/Data/SEG_axi_cfg_register_0_reg0]

	current_bd_instance $bd

}
