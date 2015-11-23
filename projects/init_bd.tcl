proc init_bd {board_preset xadc_name} {

  # Create processing_system7
  cell xilinx.com:ip:processing_system7:5.5 ${::ps_name} \
    [list PCW_IMPORT_BOARD_PRESET $board_preset PCW_USE_S_AXI_HP0 0] \
    [list M_AXI_GP0_ACLK ${::ps_name}/FCLK_CLK0]

  # Create all required interconnections
  apply_bd_automation -rule xilinx.com:bd_rule:processing_system7 -config {
		make_external {FIXED_IO, DDR}
		Master Disable
		Slave Disable
	} [get_bd_cells ${::ps_name}]

  # Add XADC Wizard and AXI Interconnect
  create_bd_cell -type ip -vlnv xilinx.com:ip:xadc_wiz:3.2 $xadc_name

	apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config \
    [list Master "/${::ps_name}/M_AXI_GP0" Clk "Auto"] \
    [get_bd_intf_pins $xadc_name/s_axi_lite]

	properties $xadc_name {
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
	}

	foreach {port_name} {
		Vp_Vn
		Vaux0
		Vaux1
		Vaux8
		Vaux9
	} {
		connect_bd_intf_net [get_bd_intf_pins $xadc_name/$port_name] [get_bd_intf_ports $port_name]
	}

  set_property -dict [list CONFIG.S00_HAS_REGSLICE 4] [get_bd_cells ${::ps_name}_axi_periph]
}

proc add_master_interface {} {
	# Add a new Master Interface to AXI Interconnect
	set num_master_interfaces [get_property CONFIG.NUM_MI [get_bd_cells ${::ps_name}_axi_periph]]
	incr num_master_interfaces
	properties ${::ps_name}_axi_periph [list NUM_MI $num_master_interfaces]
}
