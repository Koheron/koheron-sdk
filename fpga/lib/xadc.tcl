
proc add_xadc {name {idx "auto"} {intercon_idx 0}} {

  if { $idx eq "auto"} {
    # Add a new Master Interface to AXI Interconnect
    set idx [add_master_interface $intercon_idx]
  }

  cell xilinx.com:ip:axi_clock_converter:2.1 axi_clk_conv_xadc {} {
    s_axi_aclk [set ::ps_clk$intercon_idx]
    s_axi_aresetn [set ::rst${intercon_idx}_name]/peripheral_aresetn
    m_axi_aclk $::adc_clk
    m_axi_aresetn $::rst_adc_clk_name/peripheral_aresetn
  }
 
  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins axi_mem_intercon_$intercon_idx/M${idx}_AXI] [get_bd_intf_pins axi_clk_conv_xadc/S_AXI]

  cell xilinx.com:ip:xadc_wiz:3.3 $name {
    XADC_STARUP_SELECTION        independent_adc
    CHANNEL_ENABLE_VAUXP0_VAUXN0 true
    CHANNEL_ENABLE_VAUXP1_VAUXN1 true
    CHANNEL_ENABLE_VAUXP8_VAUXN8 true
    CHANNEL_ENABLE_VAUXP9_VAUXN9 true
    CHANNEL_ENABLE_VP_VN         true
    DCLK_FREQUENCY               125
    ADC_CONVERSION_RATE          1000
  } {
    s_axi_aclk $::adc_clk
    s_axi_aresetn $::rst_adc_clk_name/peripheral_aresetn
  }

  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins axi_clk_conv_xadc/M_AXI] [get_bd_intf_pins $name/s_axi_lite]

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
