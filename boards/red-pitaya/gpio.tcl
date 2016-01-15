
proc add_gpio {} {

	add_master_interface

	set gpio_name axi_gpio_0
	cell xilinx.com:ip:axi_gpio:2.0 $gpio_name {
		C_GPIO_WIDTH 6
		C_GPIO2_WIDTH 6
		C_IS_DUAL 1
	} {}

	apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config [list Master "/${::ps_name}/M_AXI_GP0" Clk "Auto"]  [get_bd_intf_pins $gpio_name/S_AXI]
	apply_bd_automation -rule xilinx.com:bd_rule:board  [get_bd_intf_pins $gpio_name/GPIO]
	apply_bd_automation -rule xilinx.com:bd_rule:board  [get_bd_intf_pins $gpio_name/GPIO2]
	set_property name exp_n [get_bd_intf_ports gpio_rtl]
	set_property name exp_p [get_bd_intf_ports gpio_rtl_0]

}
