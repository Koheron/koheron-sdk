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

proc get_constant_pin {value width} {
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



