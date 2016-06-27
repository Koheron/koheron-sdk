proc add_config_register {module_name clk {num_ports 32} {range 4K} {offset "auto"} {idx "auto"} {intercon_idx 0}} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir O -from [expr $num_ports*32] -to 0 cfg

  for {set i 0} {$i < $num_ports} {incr i} {
    create_bd_pin -dir O -from 31 -to 0 Out$i
  }

  if { $idx eq "auto"} {
    # Add a new Master Interface to AXI Interconnect
    set idx [add_master_interface $intercon_idx]
  }

  # AXI clock converter
  cell xilinx.com:ip:axi_clock_converter:2.1 axi_clock_converter_0 {} {
    s_axi_aclk /[set ::ps_clk$intercon_idx]
    s_axi_aresetn /[set ::rst${intercon_idx}_name]/peripheral_aresetn
    m_axi_aclk    /$clk
    m_axi_aresetn /${::rst_adc_clk_name}/peripheral_aresetn
  }
  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins /axi_mem_intercon_$intercon_idx/M${idx}_AXI] [get_bd_intf_pins axi_clock_converter_0/S_AXI]
  
  # Cfg register
  cell pavel-demin:user:axi_cfg_register:1.0 axi_cfg_register_0 {
    CFG_DATA_WIDTH [expr $num_ports*32]
  } {
    aclk axi_clock_converter_0/m_axi_aclk
    aresetn /${::rst_adc_clk_name}/peripheral_aresetn
    cfg_data cfg
  }

  connect_bd_intf_net [get_bd_intf_pins axi_cfg_register_0/S_AXI] [get_bd_intf_pins axi_clock_converter_0/M_AXI]

  assign_bd_address [get_bd_addr_segs {axi_cfg_register_0/s_axi/reg0 }]
  set memory_segment [get_bd_addr_segs /${::ps_name}/Data/SEG_axi_cfg_register_0_reg0]
  set_property range $range $memory_segment

  if { $offset ne "auto"} {
    set_property offset $offset $memory_segment
  }

  for {set i 0} {$i < $num_ports} {incr i} {
    set wid  [expr $num_ports*32]
    set from [expr 31+$i*32]
    set to   [expr $i*32]
    connect_pins Out$i [get_slice_pin axi_cfg_register_0/cfg_data $from $to]
  }

  current_bd_instance $bd

}
