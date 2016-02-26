proc add_master_interface {} {
  # Add a new Master Interface to AXI Interconnect
  set num_mi [get_property CONFIG.NUM_MI [get_bd_cells /axi_mem_intercon]]
  if { $num_mi < 10 } {
    set idx 0$num_mi
  } else {
    set idx $num_mi
  }
  incr num_mi
  puts "Increasing number of master interfaces to $num_mi"
  puts "Connect your AXI Slave to M${idx}_AXI"
  set_property -dict [list CONFIG.NUM_MI $num_mi] [get_bd_cells /axi_mem_intercon]
  connect_pins /axi_mem_intercon/M${idx}_ACLK    /${::ps_name}/FCLK_CLK0
  connect_pins /axi_mem_intercon/M${idx}_ARESETN /${::rst_name}/peripheral_aresetn
  return $idx
}

proc connect_pins {pin1 pin2} {
  connect_bd_net [get_bd_pins $pin1] [get_bd_pins $pin2]
}

proc connect_constant {name value width pin} {
  cell xilinx.com:ip:xlconstant:1.1 $name \
    [list CONST_VAL $value CONST_WIDTH $width] \
    [list dout $pin]
}
