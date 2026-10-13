# Same GPIO interface names as the original Red Pitaya board, without board
# automation (these BSPs use explicit presets rather than Vivado board files).
proc add_gpio {{gpio_width 6} {idx "auto"} {intercon_idx 0}} {
  set max_width 8
  if {[get_property PART [current_project]] eq "xc7z020clg400-1"} {
    set max_width 11
  }
  if {$gpio_width < 1 || $gpio_width > $max_width} {
    error "This Red Pitaya board supports 1..$max_width GPIOs per side"
  }
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
  connect_bd_intf_net [get_bd_intf_pins axi_mem_intercon_$intercon_idx/M${idx}_AXI] [get_bd_intf_pins $gpio_name/S_AXI]
  foreach {pin port} {GPIO exp_n GPIO2 exp_p} {
    create_bd_intf_port -mode Master -vlnv xilinx.com:interface:gpio_rtl:1.0 $port
    connect_bd_intf_net [get_bd_intf_ports $port] [get_bd_intf_pins $gpio_name/$pin]
  }
}
