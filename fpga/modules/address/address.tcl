namespace eval address {

proc pins {cmd addr_width {n_periods 1}} {
  $cmd -dir I -from 31   -to 0 cfg
  for {set i 0} {$i < $n_periods} {incr i} {
    $cmd -dir I -from 31   -to 0 period$i
    $cmd -dir O -from [expr $addr_width + 1] -to 0 addr$i
  }
  $cmd -dir O -from 31   -to 0 restart
  $cmd -dir O -from 0    -to 0 tvalid
  $cmd -dir I -type clk        clk
}

proc create {module_name addr_width {n_periods 1}} {
  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  pins create_bd_pin $addr_width $n_periods
  
  # Configuration registers
  set reset_pin [get_slice_pin cfg 0 0]
  set reset_acq_pin [get_slice_pin cfg 1 1]

  connect_pin restart [get_edge_detector_pin $reset_acq_pin]

  # Add address counter

  for {set i 0} {$i < $n_periods} {incr i} {
    cell koheron:user:address_generator:1.0 base_counter$i {
      COUNT_WIDTH $addr_width
    } {
      clk clk
      count_max period$i
      address addr$i
      sclr [get_not_pin $reset_pin]
    }
  }

  set clogb2_depth 5
  set depth [expr 2**$clogb2_depth]
  set delay_pin [get_slice_pin cfg [expr 2 + $clogb2_depth -1] 2]

  cell xilinx.com:ip:c_shift_ram:12.0 delay_tvalid {
    ShiftRegType Variable_Length_Lossless
    Depth [expr 2**$clogb2_depth]
    Width 1
  } {
    D $reset_pin
    CLK clk
    Q tvalid
    A $delay_pin
  }

  current_bd_instance $bd
}

} ;# end address namespace
