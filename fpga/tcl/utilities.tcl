proc cell {cell_vlnv cell_name {cell_props {}} {cell_ports {}}} {
  set cell [create_bd_cell -type ip -vlnv $cell_vlnv $cell_name]
  set prop_list {}
  foreach {prop_name prop_value} [uplevel 1 [list subst $cell_props]] {
    lappend prop_list CONFIG.$prop_name $prop_value
  }
  if {[llength $prop_list] > 1} {
    set_property -dict $prop_list $cell
  }
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

proc module {module_name module_body {module_ports {}}} {
  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]
  eval $module_body
  current_bd_instance $bd
  foreach {local_name remote_name} [uplevel 1 [list subst $module_ports] {
    set local_port [get_bd_pins $module_name/$local_name]
    set remote_port [get_bd_pins $remote_name]
    if {[llength $local_port] == 1 && [llength $remote_port] == 1} {
      connect_bd_net $local_port $remote_port
      continue
    }
    set local_port [get_bd_intf_pins $module_name/$local_name]
    set remote_port [get_bd_intf_pins $remote_name]
    if {[llength $local_port] == 1 && [llength $remote_port] == 1} {
      connect_bd_intf_net $local_port $remote_port
      continue
    }
    error "** ERROR: can't connect $module_name/$local_name and $remote_name"
  }
}

proc properties {cell_name {cell_props {}}} {
  set prop_list {}
  foreach {prop_name prop_value} [uplevel 1 [list subst $cell_props]] {
    lappend prop_list CONFIG.$prop_name $prop_value
  }
  if {[llength $prop_list] > 1} {
    set_property -dict $prop_list [get_bd_cells $cell_name]
  }
}

proc add_master_interface {{intercon_idx 0}} {

  set num_mi [get_property CONFIG.NUM_MI [get_bd_cells /axi_mem_intercon_$intercon_idx]]

  # Look for an empty interface
  for {set i [expr $num_mi]} {$i > 0} {incr i -1} {
    if { $i < 10 } { set idx 0[expr $i-1] } { set idx [expr $i-1] }
    set net [get_bd_intf_nets -of_objects [get_bd_intf_pins /axi_mem_intercon_${intercon_idx}/M${idx}_AXI]]
    if {$net eq ""} {
      puts "Found empty interface M${idx}_AXI on interconnect $intercon_idx..."
      connect_pins /axi_mem_intercon_$intercon_idx/M${idx}_ACLK    /[set ::ps_clk$intercon_idx]
      connect_pins /axi_mem_intercon_$intercon_idx/M${idx}_ARESETN /[set ::rst${intercon_idx}_name]/peripheral_aresetn 
      puts "Connect your AXI Slave to axi_mem_intercon_$intercon_idx/M${idx}_AXI"
      return $idx
    }    
  }
  # No empty interface :
  puts "Increasing number of master interfaces to $num_mi on interconnect $intercon_idx..."
  incr num_mi
  set_property -dict [list CONFIG.NUM_MI $num_mi] [get_bd_cells /axi_mem_intercon_$intercon_idx]
  if { $num_mi < 10 } { set idx 0[expr $num_mi-1] } { set idx [expr $num_mi-1] }
  connect_pins /axi_mem_intercon_$intercon_idx/M${idx}_ACLK    /[set ::ps_clk$intercon_idx]
  connect_pins /axi_mem_intercon_$intercon_idx/M${idx}_ARESETN /[set ::rst${intercon_idx}_name]/peripheral_aresetn 
  puts "Connect your AXI Slave to axi_mem_intercon_$intercon_idx/M${idx}_AXI"
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

proc cfg_pin {name} {
  return $::config_name/Out[set config::${name}_offset]
}

proc sts_pin {name} {
  return $::status_name/In[set config::${name}_offset]
}

