
proc add_xadc {name} {

  # Add a new Master Interface to AXI Interconnect
  set num_mi [get_property CONFIG.NUM_MI [get_bd_cells axi_mem_intercon]]
  if { $num_mi < 10 } {
    set idx 0$num_mi
  } else {
    set idx $num_mi
  }
  incr num_mi
  properties axi_mem_intercon [list NUM_MI $num_mi]

  cell xilinx.com:ip:xadc_wiz:3.2 $name {
    XADC_STARUP_SELECTION        independent_adc
    OT_ALARM                     false
    USER_TEMP_ALARM              false
    VCCINT_ALARM                 false
    VCCAUX_ALARM                 false
    ENABLE_VCCPINT_ALARM         false
    ENABLE_VCCPAUX_ALARM         false
    ENABLE_VCCDDRO_ALARM         false
    CHANNEL_ENABLE_VAUXP0_VAUXN0 true
    CHANNEL_ENABLE_VAUXP1_VAUXN1 true
    CHANNEL_ENABLE_VAUXP8_VAUXN8 true
    CHANNEL_ENABLE_VAUXP9_VAUXN9 true
    CHANNEL_ENABLE_VP_VN         true
    AVERAGE_ENABLE_VAUXP0_VAUXN0 true
    AVERAGE_ENABLE_VAUXP1_VAUXN1 true
    AVERAGE_ENABLE_VAUXP8_VAUXN8 true
    AVERAGE_ENABLE_VAUXP9_VAUXN9 true
    AVERAGE_ENABLE_VP_VN         true
  } {}

  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins axi_mem_intercon/M${idx}_AXI] [get_bd_intf_pins $name/s_axi_lite]

  connect_bd_net [get_bd_pins axi_mem_intercon/M${idx}_ACLK] [get_bd_pins ${::ps_name}/FCLK_CLK0]
  connect_bd_net [get_bd_pins axi_mem_intercon/M${idx}_ARESETN] [get_bd_pins ${::rst_name}/peripheral_aresetn]

  connect_bd_net [get_bd_pins $name/s_axi_aclk] [get_bd_pins ${::ps_name}/FCLK_CLK0]
  connect_bd_net [get_bd_pins $name/s_axi_aresetn] [get_bd_pins ${::rst_name}/peripheral_aresetn]

  foreach {port_name} {
    Vp_Vn
    Vaux0
    Vaux1
    Vaux8
    Vaux9
  } {
    connect_bd_intf_net [get_bd_intf_pins $name/$port_name] [get_bd_intf_ports $port_name]
  }

  assign_bd_address [get_bd_addr_segs $name/s_axi_lite/Reg ]

}
