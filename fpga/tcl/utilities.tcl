########################################################
# Helper functions
########################################################

# Get a configuration pin
# name : name of the register defined in the project YAML
proc cfg_pin {name} {
  return $::config_name/Out[set config::${name}_offset]
}

# Get a status pin
proc sts_pin {name} {
  return $::status_name/In[set config::${name}_offset]
}

proc get_not_pin {pin_name} {
  # TODO use function here ...
  set i 0
  while 1 {
    set not_name not_[lindex [split $pin_name /] end]_${i}
    if {[get_bd_cells $not_name] eq ""} {break}
    incr i
  }
  cell xilinx.com:ip:util_vector_logic:2.0 $not_name {
    C_SIZE 1
    C_OPERATION not
  } {
    Op1 $pin_name
  }
  return $not_name/Res
}

proc get_slice_pin {pin_name width from to} {
  # TODO ... and here
  set i 0
  while 1 {
    set slice_name slice_from${from}_to${to}_[lindex [split $pin_name /] end]_${i}
    if {[get_bd_cells $slice_name] eq ""} {break}
    incr i
  }
  cell xilinx.com:ip:xlslice:1.0 $slice_name {
    DIN_WIDTH $width
    DIN_FROM $from
    DIN_TO $to
  } {
    Din $pin_name
  }
  return $slice_name/Dout
}

proc get_edge_detector_pin {pin_name {clk clk}} {
  # TODO ... and here
  set i 0
  while 1 {
    set edge_det_name edge_detector_[lindex [split $pin_name /] end]_${i}
    if {[get_bd_cells $edge_det_name] eq ""} {break}
    incr i
  }
  cell koheron:user:edge_detector:1.0 $edge_det_name {} {
    clk $clk
    din $pin_name
  }
  return $edge_det_name/dout
}

proc get_Q_pin {in_pin_name {width 1} {depth 1} {ce_pin_name "ce_false"} {clk clk}} {
  # TODO ... and here
  set i 0
  while 1 {
    set shift_reg_name Q_d${depth}_[lindex [split $in_pin_name /] end]_${i}
    if {[get_bd_cells $shift_reg_name] eq ""} {break}
    incr i
  }
  if { [string match "ce_false" $ce_pin_name] } {
    cell xilinx.com:ip:c_shift_ram:12.0 $shift_reg_name {
      Width.VALUE_SRC USER
      Width $width
      Depth $depth
    } {
      CLK $clk
      D   $in_pin_name
    }
  } else {
    cell xilinx.com:ip:c_shift_ram:12.0 $shift_reg_name {
      Width.VALUE_SRC USER
      Width $width
      Depth $depth
      CE true
    } {
      CLK clk
      D   $in_pin_name
      CE  $ce_pin_name
    }
  }
  return $shift_reg_name/Q
}

proc get_constant_pin {value width} {
  # TODO ... and here
  set i 0
  while 1 {
    set const_name const${i}_v${value}_w${width}
    if {[get_bd_cells $const_name] eq ""} {break}
    incr i
  }
  cell xilinx.com:ip:xlconstant:1.1 $const_name {
    CONST_VAL $value
    CONST_WIDTH $width
  } {}
  return $const_name/dout
}

proc connect_pins {pin1 pin2} {
  connect_bd_net [get_bd_pins $pin1] [get_bd_pins $pin2]
}

proc connect_constant {name value width pin} {
  cell xilinx.com:ip:xlconstant:1.1 $name {
    CONST_VAL $value
    CONST_WIDTH $width
  } { 
    dout $pin
  }
}

# http://wiki.tcl.tk/13920
proc lmap {_var list body} {
    upvar 1 $_var var
    set res {}
    foreach var $list {lappend res [uplevel 1 $body]}
    set res
}

# Connect all the pins of a cell that have a port with an identical name
proc connect_ports {cell_name} {
  set cell_pins [lmap pin [get_bd_pins $cell_name/*] {set pin [lindex [split $pin /] end]}]
  set ports     [lmap pin [get_bd_ports /*]          {set pin [lindex [split $pin /] end]}]
  package require struct::set
  set common_ports [::struct::set intersect $cell_pins $ports]
  foreach port $common_ports {
    connect_bd_net [get_bd_ports /$port] [get_bd_pins $cell_name/$port]
  }
}

# Configure an IP block and connect its pins 
# https://github.com/pavel-demin/red-pitaya-notes

proc connect_cell {cell_name cell_ports} {
  foreach {local_name remote_name} [uplevel 1 [list subst $cell_ports]] {
    set local_port [get_bd_pins $cell_name/$local_name]
    set remote_port [get_bd_pins $remote_name]
    if {[llength $local_port] == 1 && [llength $remote_port] == 1} {
      connect_bd_net $local_port $remote_port
      continue
    }
    set local_port [get_bd_intf_pins $cell_name/$local_name]
    set remote_port [get_bd_intf_pins $remote_name]
    if {[llength $local_port] == 1 && [llength $remote_port] == 1} {
      connect_bd_intf_net $local_port $remote_port
      continue
    }
    error "** ERROR: can't connect $cell_name/$local_name and $remote_name"
  }
}

proc set_cell_props {cell_name cell_props} {
  set prop_list {}
  foreach {prop_name prop_value} [uplevel 1 [list subst $cell_props]] {
    lappend prop_list CONFIG.$prop_name $prop_value
  }
  if {[llength $prop_list] > 1} {
    set_property -dict $prop_list [get_bd_cell $cell_name]
  }
}

proc cell {cell_vlnv cell_name {cell_props {}} {cell_ports {}}} {
  create_bd_cell -type ip -vlnv $cell_vlnv $cell_name
  set_cell_props $cell_name [uplevel 1 [list subst $cell_props]]
  connect_cell   $cell_name [uplevel 1 [list subst $cell_ports]]
}

########################################################
# Add AXI Master Interface to memory interconnect
########################################################

proc add_master_interface {{intercon_idx 0}} {
  # Creates an empty M_${idx}_AXI interface in interconnect $intercon_idx
  # Return $idx

  set num_mi [get_property CONFIG.NUM_MI [get_bd_cells /axi_mem_intercon_$intercon_idx]]

  # Look for an already empty interface
  set found 0
  for {set i [expr $num_mi]} {$i > 0} {incr i -1} {
    if { $i < 10 } { set idx 0[expr $i-1] } { set idx [expr $i-1] }
    set net [get_bd_intf_nets -of_objects [get_bd_intf_pins /axi_mem_intercon_${intercon_idx}/M${idx}_AXI]]
    if {$net eq ""} {
      puts "Found empty interface M${idx}_AXI on interconnect $intercon_idx..."
      set found 1
      break
    }
  }
  if {$found == 0} {
    puts "No empty interface found on interconnect $intercon_idx..."
    incr num_mi
    set_property -dict [list CONFIG.NUM_MI $num_mi] [get_bd_cells /axi_mem_intercon_$intercon_idx]
    if { $num_mi < 10 } { set idx 0[expr $num_mi-1] } { set idx [expr $num_mi-1] }
  }
  connect_pins /axi_mem_intercon_$intercon_idx/M${idx}_ACLK    /[set ::ps_clk$intercon_idx]
  connect_pins /axi_mem_intercon_$intercon_idx/M${idx}_ARESETN /[set ::rst${intercon_idx}_name]/peripheral_aresetn 
  puts "Connect your AXI Slave to axi_mem_intercon_$intercon_idx/M${idx}_AXI"
  return $idx
}



