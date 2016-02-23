proc add_master_interface {} {
  # Add a new Master Interface to AXI Interconnect
  set num_master_interfaces [get_property CONFIG.NUM_MI [get_bd_cells axi_mem_intercon]]
  incr num_master_interfaces
  properties axi_mem_intercon [list NUM_MI $num_master_interfaces]
}

proc connect_pins {pin1 pin2} {
  connect_bd_net [get_bd_pins $pin1] [get_bd_pins $pin2]
}

proc connect_constant {name value width pin} {
  cell xilinx.com:ip:xlconstant:1.1 $name \
    [list CONST_VAL $value CONST_WIDTH $width] \
    [list dout $pin]
}
