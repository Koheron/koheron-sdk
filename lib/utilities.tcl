
proc connect_pins {pin1 pin2} {
  connect_bd_net [get_bd_pins $pin1] [get_bd_pins $pin2]
}

proc connect_constant {name value width pin} {
  cell xilinx.com:ip:xlconstant:1.1 $name \
    [list CONST_VAL $value CONST_WIDTH $width] \
    [list dout $pin]
}

