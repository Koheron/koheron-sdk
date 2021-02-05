proc add_ctl_sts {{mclk "None"}  {mrstn "None"}} {
  add_config_register ctl control $mclk $mrstn config::ctl_register $config::control_size
  add_status_register sts status $mclk $mrstn config::sts_register $config::status_size

  if {$config::ps_control_size > 0} {
    add_config_register ps_ctl ps_control "None" "None" config::ps_ctl_register $config::ps_control_size
  }

  if {$config::ps_status_size > 0} {
    add_status_register ps_sts ps_status "None" "None" config::ps_sts_register $config::ps_status_size 0 0
  }
}

proc add_config_register {module_name memory_name mclk mrstn reg_names {num_ports 32} {intercon_idx 0}} {
  upvar 1 $reg_names register

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  for {set i 0} {$i < $num_ports} {incr i} {
    create_bd_pin -dir O -from 31 -to 0 $register($i)
  }

  # Add a new Master Interface to AXI Interconnect
  set idx [add_master_interface $intercon_idx]

  set sclk [set ::ps_clk$intercon_idx]
  set srstn [set ::rst${intercon_idx}_name]/peripheral_aresetn

  if {$mclk eq "None"} {
    set mclk $sclk
    set mrstn $srstn
  }

  if {$sclk != $mclk} {
    # Add AXI clock converter
    cell xilinx.com:ip:axi_clock_converter:2.1 axi_clock_converter_0 {} {
      s_axi_aclk /$sclk
      s_axi_aresetn /$srstn
      m_axi_aclk /$mclk
      m_axi_aresetn /$mrstn
      S_AXI /axi_mem_intercon_$intercon_idx/M${idx}_AXI
    }
    set m_axi_aclk axi_clock_converter_0/m_axi_aclk
    set M_AXI axi_clock_converter_0/M_AXI
  } else {
    set m_axi_aclk /$sclk
    set M_AXI /axi_mem_intercon_$intercon_idx/M${idx}_AXI
  }

    # Cfg register
  cell pavel-demin:user:axi_ctl_register:1.0 axi_${module_name}_register {
    CTL_DATA_WIDTH [expr $num_ports*32]
  } {
    aclk $m_axi_aclk
    aresetn /$mrstn
    S_AXI $M_AXI
  }

  assign_bd_address [get_bd_addr_segs {axi_${module_name}_register/s_axi/reg0 }]
  set memory_segment [get_bd_addr_segs /${::ps_name}/Data/SEG_axi_${module_name}_register_reg0]
  set_property range  [get_memory_range $memory_name]  $memory_segment
  set_property offset [get_memory_offset $memory_name] $memory_segment

  for {set i 0} {$i < $num_ports} {incr i} {
    set wid  [expr $num_ports*32]
    set from [expr 31+$i*32]
    set to   [expr $i*32]
    connect_pins $register($i) [get_slice_pin axi_${module_name}_register/ctl_data $from $to]
  }

  current_bd_instance $bd
}

proc add_status_register {module_name memory_name mclk mrstn reg_names {num_ports 32} {intercon_idx 0} {has_dna 1}} {
  upvar 1 $reg_names register

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

<<<<<<< HEAD
  for {set i 0} {$i < $num_ports} {incr i} {
=======
  if {$has_dna == 1} {
    set n_hidden_ports 2
  } else {
    set n_hidden_ports 0
  }

  for {set i $n_hidden_ports} {$i < $num_ports} {incr i} {
>>>>>>> d8bf2165890c46bf522f1053ba7f3597f99ddbb7
    create_bd_pin -dir I -from 31 -to 0 $register($i)
  }

  # Add a new Master Interface to AXI Interconnect
  set idx [add_master_interface $intercon_idx]

  set sclk [set ::ps_clk$intercon_idx]
  set srstn [set ::rst${intercon_idx}_name]/peripheral_aresetn

  if {$mclk eq "None"} {
    set mclk $sclk
    set mrstn $srstn
  }

  if {$sclk != $mclk} {
    # Add AXI clock converter
    cell xilinx.com:ip:axi_clock_converter:2.1 axi_clock_converter_0 {} {
      s_axi_aclk /$sclk
      s_axi_aresetn /$srstn
      m_axi_aclk /$mclk
      m_axi_aresetn /$mrstn
      S_AXI /axi_mem_intercon_$intercon_idx/M${idx}_AXI
    }
    set m_axi_aclk axi_clock_converter_0/m_axi_aclk
    set M_AXI axi_clock_converter_0/M_AXI
  } else {
    set m_axi_aclk /$sclk
    set M_AXI /axi_mem_intercon_$intercon_idx/M${idx}_AXI
  }

  # Sts register
  cell pavel-demin:user:axi_sts_register:1.0 axi_${module_name}_register_0 {
    STS_DATA_WIDTH [expr $num_ports*32]
  } {
    aclk $m_axi_aclk
    aresetn /$mrstn
    S_AXI $M_AXI
  }

  assign_bd_address [get_bd_addr_segs {axi_${module_name}_register_0/s_axi/reg0 }]
  set memory_segment [get_bd_addr_segs /${::ps_name}/Data/SEG_axi_${module_name}_register_0_reg0]
  set_property range  [get_memory_range $memory_name]  $memory_segment
  set_property offset [get_memory_offset $memory_name] $memory_segment

<<<<<<< HEAD
=======
  if {$has_dna == 1} {
    # DNA (hidden ports)
    cell pavel-demin:user:dna_reader:1.0 dna {} {
      aclk /$mclk
      aresetn /$mrstn
    }
  }

>>>>>>> d8bf2165890c46bf522f1053ba7f3597f99ddbb7
  set left_ports $num_ports
  set concat_idx 0

  # Use multiple concats because Xilinx IP does not allow num_ports > 32
  set concat_num [expr $num_ports / 32 + 1]
  # Concat the multiple concat blocks
  cell xilinx.com:ip:xlconcat:2.1 concat_concat {
    NUM_PORTS $concat_num
  } {
    dout axi_${module_name}_register_0/sts_data
  }

  while {$left_ports > 0} {
    set concat_num_ports [expr min($left_ports, 32)]

    cell xilinx.com:ip:xlconcat:2.1 concat_$concat_idx {
      NUM_PORTS $concat_num_ports
    } {
      dout concat_concat/In$concat_idx
    }
    set_property -dict [list CONFIG.IN${concat_idx}_WIDTH [expr $concat_num_ports * 32]] [get_bd_cells concat_concat]
    for {set i 0} {$i < $num_ports} {incr i} {
      set_property -dict [list CONFIG.IN${i}_WIDTH 32] [get_bd_cells concat_$concat_idx]
    }
    set left_ports [expr $left_ports - $concat_num_ports]
    incr concat_idx
  }

<<<<<<< HEAD
=======
  if {$has_dna == 1} {
    connect_pins concat_0/In0 [get_slice_pin dna/dna_data 31 0]
    connect_pins concat_0/In1 [get_slice_pin dna/dna_data 56 32]
  }

>>>>>>> d8bf2165890c46bf522f1053ba7f3597f99ddbb7
  # Other ports
  for {set i 0} {$i < $num_ports} {incr i} {
    set iidx [expr $i % 32]
    set cidx [expr $i / 32]
    connect_pins concat_$cidx/In$iidx $register($i)
  }

  current_bd_instance $bd
}
