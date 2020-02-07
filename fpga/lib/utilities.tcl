########################################################
# Helper functions
########################################################

# http://wiki.tcl.tk/13920
proc lmap {_var list body} {
    upvar 1 $_var var
    set res {}
    foreach var $list {lappend res [uplevel 1 $body]}
    set res
}

proc range {from to} {
    if {$to>$from} {concat [range $from [incr to -1]] $to}
 }

# Get a configuration pin
# name : name of the register defined in the instrument YAML
proc ctl_pin {pin_name} {
  return ctl/$pin_name
}


# Get a PS configuration pin
# name : name of the register defined in the instrument YAML
proc ps_ctl_pin {pin_name} {
  return ps_ctl/$pin_name
}


# Get a status pin
proc sts_pin {pin_name} {
  return sts/$pin_name
}

# Get a parameter defined in main.yml
proc get_parameter {param_name} {
  return [set ::config::$param_name]
}

proc underscore {pin_name} {
  return [join [split $pin_name /] _]
}

proc get_cell_name {op pin_name1 {pin_name2 ""}} {
  if {$pin_name2 eq ""} {
    return ${op}_[underscore $pin_name1]
  } else {
    return [underscore $pin_name1]_${op}_[underscore $pin_name2]
  }
}

proc get_pin_width {pin_name} {
  set left  [get_property LEFT  [get_bd_pins $pin_name]]
  set right [get_property RIGHT [get_bd_pins $pin_name]]
  set width [expr $left - $right + 1]
  if {$width < 1} {return 1} else {return $width}
}

proc get_concat_pin {pins {cell_name ""}} {
  set pin_names [uplevel 1 [list subst $pins]]
  if {$cell_name eq ""} {
    set cell_name concat_[join [lmap pin $pin_names {set pin [lindex [split $pin /] end]}] _]
  }
  if {[get_bd_cells $cell_name] eq ""} {
    cell xilinx.com:ip:xlconcat:2.1 $cell_name {
      NUM_PORTS [llength $pin_names]
    } {}
  }
  set i 0
  foreach {pin_name} [uplevel 1 [list subst $pins]] {
    connect_pins $cell_name/In$i $pin_name
    set_cell_props $cell_name {IN${i}_WIDTH [get_pin_width $pin_name]}
    incr i
  }
  return $cell_name/dout
}

# define get_and_pin, get_or_pin, get_nor_pin and get_not_pin procedures
foreach op {and or nor not} {
  proc get_${op}_pin {pin_name1 {pin_name2 ""} {cell_name ""}} {
    set proc_name [lindex [info level 0] 0]
    set op [lindex [split $proc_name _] 1]
    if {$cell_name eq ""} {
      set cell_name [get_cell_name $op $pin_name1 $pin_name2]
    }
    if {[get_bd_cells $cell_name] eq ""} {
      cell xilinx.com:ip:util_vector_logic:2.0 $cell_name {
        C_SIZE [get_pin_width $pin_name1]
        C_OPERATION $op
      } {
        Op1 $pin_name1
      }
      if {$pin_name2 ne ""} {connect_pins $cell_name/Op2 $pin_name2}
    }
    return $cell_name/Res
  }
}

foreach op {GE GT LE LT EQ NE} {
  proc get_${op}_pin {pin_name1 pin_name2 {cell_name ""}} {
    set proc_name [lindex [info level 0] 0]
    set op [lindex [split $proc_name _] 1]
    if {$cell_name eq ""} {
      set cell_name [get_cell_name $op $pin_name1 $pin_name2]
    }
    if {[get_bd_cells $cell_name] eq ""} {
      cell koheron:user:comparator:1.0 $cell_name {
        DATA_WIDTH [get_pin_width $pin_name1]
        OPERATION $op
      } {
        a $pin_name1
        b $pin_name2
      }
    }
    return $cell_name/dout
  }
}

proc get_slice_pin {pin_name from to {cell_name ""}} {
  if {$cell_name eq ""} {
    set cell_name slice_${from}_${to}_[underscore $pin_name]
  }
  if {[get_bd_cells $cell_name] eq ""} {
    cell xilinx.com:ip:xlslice:1.0 $cell_name {
      DIN_WIDTH [get_pin_width $pin_name]
      DIN_FROM $from
      DIN_TO $to
    } {
      Din $pin_name
    }
  }
  return $cell_name/Dout
}

proc get_Q_pin {pin_name {depth 1} {ce_pin_name "noce"} {clk clk} {cell_name ""}} {
  if {$cell_name eq ""} {
    set cell_name Q_d${depth}_[underscore $ce_pin_name]_[underscore $pin_name]
  }
  set width [get_pin_width $pin_name]
  if {[get_bd_cells $cell_name] eq ""} {
    if { [string match "noce" $ce_pin_name] } {
      cell xilinx.com:ip:c_shift_ram:12.0 $cell_name {
        Width $width
        Depth $depth
      } {
        CLK $clk
        D   $pin_name
      }
    } else {
      cell xilinx.com:ip:c_shift_ram:12.0 $cell_name {
        Width $width
        Depth $depth
        CE true
      } {
        CLK $clk
        D   $pin_name
        CE  $ce_pin_name
      }
    }
  }
  return $cell_name/Q
}

proc get_constant_pin {value width} {
  set cell_name const_v${value}_w${width}
  if {[get_bd_cells $cell_name] eq ""} {
    cell xilinx.com:ip:xlconstant:1.1 $cell_name {
      CONST_VAL $value
      CONST_WIDTH $width
    } {}
  }
  return $cell_name/dout
}

# see fpga/cores/edge_detector_v1_0
proc get_edge_detector_pin {pin_name {clk clk}} {
  set cell_name edge_detector_[lindex [split $pin_name /] end]
  if {[get_bd_cells $cell_name] eq ""} {
    cell koheron:user:edge_detector:1.0 $cell_name {} {
      clk $clk
      din $pin_name
    }
  }
  return $cell_name/dout
}


proc connect_pins {pin1 pin2} {
  connect_bd_net [get_bd_pins $pin1] [get_bd_pins $pin2]
}

proc connect_port_pin {port pin} {
  connect_bd_net [get_bd_ports $port] [get_bd_pins $pin]
}

proc connect_constant {name value width pin} {
  cell xilinx.com:ip:xlconstant:1.1 $name {
    CONST_VAL $value
    CONST_WIDTH $width
  } {
    dout $pin
  }
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
  create_bd_cell -type ip -vlnv [check_vlnv $cell_vlnv] $cell_name
  set_cell_props $cell_name [uplevel 1 [list subst $cell_props]]
  connect_cell   $cell_name [uplevel 1 [list subst $cell_ports]]
}

########################################################
# IP managment
########################################################

proc check_vlnv {vlnv} {
  set name [get_name_from_vlnv $vlnv]
  if {$name == "axi_interconnect"} {
    # Fix bug in Vivado IP list
    return $vlnv
  }
  set version [get_version_from_vlnv $vlnv]
  set vlnvs [get_ipdefs -filter "NAME == $name"]
  foreach vlnv_ $vlnvs {
    if [string equal [get_version_from_vlnv $vlnv_] $version] {
      return $vlnv
    }
  }
  set vlnv_ [lindex $vlnvs 0]
  puts [concat "WARNING : using IP " $vlnv_ "instead of" $vlnv]
  return $vlnv_
}

proc get_name_from_vlnv {vlnv} {
  return [lindex [split $vlnv :] 2]
}

proc get_version_from_vlnv {vlnv} {
  return [lindex [split $vlnv :] 3]
}

########################################################
# Addresses
########################################################

proc get_memory_range {memory_name} {
  return [set config::memory_${memory_name}_range]
}

proc get_memory_offset {memory_name} {
  return [set config::memory_${memory_name}_offset]
}

proc get_memory_depth {memory_name {width 32}} {
  return [expr [string map {K *1024 M *1024*1024} [get_memory_range $memory_name]] * 8 / $width]
}

proc get_memory_addr_width {memory_name} {
  set depth [get_memory_depth $memory_name]
  return [expr int(ceil(log($depth)/log(2)))]
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
    if { $i <= 10 } { set idx 0[expr $i-1] } { set idx [expr $i-1] }
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
    if { $num_mi <= 10 } { set idx 0[expr $num_mi-1] } { set idx [expr $num_mi-1] }
  }
  connect_pins /axi_mem_intercon_$intercon_idx/M${idx}_ACLK    /[set ::ps_clk$intercon_idx]
  connect_pins /axi_mem_intercon_$intercon_idx/M${idx}_ARESETN /[set ::rst${intercon_idx}_name]/peripheral_aresetn
  puts "Connect your AXI Slave to axi_mem_intercon_$intercon_idx/M${idx}_AXI"
  return $idx
}