source fpga/lib/bram.tcl

# Single BRAM recorder (32 bit width)

proc add_dac_router {module_name memory_name {intercon_idx 0}} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I -from 31 -to 0 addr_select
  create_bd_pin -dir I -from 31 -to 0 dac_select
  create_bd_pin -dir I -type clk clk
  create_bd_pin -dir I           rst
  create_bd_pin -dir I           clken

  for {set i 0} {$i <  [get_parameter n_dac]} {incr i} {
    create_bd_pin -dir I -from [expr [get_memory_addr_width $memory_name] + 2] -to 0 addr$i
  }

  for {set i 0} {$i <  [get_parameter n_dac]} {incr i} {
    create_bd_pin -dir O -from [expr [get_parameter dac_width]-1] -to 0 dout$i
  }

  source fpga/lib/interconnect.tcl
  set addr_intercon_name addr_intercon
  interconnect::create $addr_intercon_name [expr [get_memory_addr_width $memory_name] + 3] [get_parameter n_dac] [get_parameter n_dac_bram]

  connect_cell $addr_intercon_name {
    clk   clk
    sel   addr_select
    clken clken
  }

  for {set i 0} {$i <  [get_parameter n_dac]} {incr i} {
    connect_pins $addr_intercon_name/in$i addr$i
  }

  # Add DAC controller

  source fpga/lib/dac_controller.tcl

  set interconnect_name dac_interconnect
  interconnect::create $interconnect_name [get_parameter dac_width] [get_parameter n_dac_bram] 2

  connect_cell $interconnect_name {
    clk clk
    sel dac_select
    clken clken
  }

  for {set i 0} {$i <  [get_parameter n_dac]} {incr i} {
    connect_pins $interconnect_name/out$i dout$i
  }

  for {set i 0} {$i < [get_parameter n_dac_bram]} {incr i} {
    set dac_controller_name dac${i}_ctrl 
    add_single_dac_controller $dac_controller_name dac$i [get_parameter dac_width] 1
    connect_cell $dac_controller_name {
      clk  clk
      addr $addr_intercon_name/out$i
      rst  rst
      dac  $interconnect_name/in$i
    }
  }

  current_bd_instance $bd

}
