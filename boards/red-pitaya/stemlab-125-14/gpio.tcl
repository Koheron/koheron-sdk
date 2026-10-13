
proc add_gpio {{gpio_width 6} {idx "auto"} {intercon_idx 0}} {

  set idx [add_master_interface $intercon_idx]
  
  set gpio_name axi_gpio_0
  cell xilinx.com:ip:axi_gpio:2.0 $gpio_name {
    C_GPIO_WIDTH $gpio_width
    C_GPIO2_WIDTH $gpio_width
    C_IS_DUAL 1
  } {
    s_axi_aclk [set ::ps_clk$intercon_idx]
    s_axi_aresetn [set ::rst${intercon_idx}_name]/peripheral_aresetn
  }

  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins /axi_mem_intercon_$intercon_idx/M${idx}_AXI] [get_bd_intf_pins $gpio_name/S_AXI]

  apply_bd_automation -rule xilinx.com:bd_rule:board  [get_bd_intf_pins $gpio_name/GPIO]
  apply_bd_automation -rule xilinx.com:bd_rule:board  [get_bd_intf_pins $gpio_name/GPIO2]
  set_property name exp_n [get_bd_intf_ports gpio_rtl]
  set_property name exp_p [get_bd_intf_ports gpio_rtl_0]
}
