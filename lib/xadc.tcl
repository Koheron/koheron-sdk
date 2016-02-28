
proc add_xadc {name {idx "auto"} {intercon_idx 0}} {

  if { $idx eq "auto"} {
    # Add a new Master Interface to AXI Interconnect
    set idx [add_master_interface $intercon_idx]
  }

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
  } {
    s_axi_aclk ${::ps_name}/FCLK_CLK0
    s_axi_aresetn ${::rst_name}/peripheral_aresetn
  }

  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins axi_mem_intercon_$intercon_idx/M${idx}_AXI] [get_bd_intf_pins $name/s_axi_lite]

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
