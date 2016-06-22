namespace eval address {

proc pins {cmd bram_width {n_periods 1}} {
  $cmd -dir I -from 31   -to 0 cfg
  for {set i 0} {$i < $n_periods} {incr i} {
    $cmd -dir I -from 31   -to 0 period$i
    $cmd -dir O -from [expr $bram_width+2] -to 0 addr$i
  }
  $cmd -dir O                  restart
  $cmd -dir O                  tvalid
  $cmd -dir I -type clk        clk
}

proc create {module_name bram_width {n_periods 1}} {
  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  pins create_bd_pin $bram_width $n_periods

  cell koheron:user:edge_detector:1.0 reset_base_counter {} {clk clk}

  # Add address counter

  for {set i 0} {$i < $n_periods} {incr i} {
    cell koheron:user:address_generator:1.0 base_counter$i {
      COUNT_WIDTH $bram_width
    } {
      clk clk
      count_max period$i
      address addr$i
      sclr reset_base_counter/dout
    }
  }

  cell koheron:user:edge_detector:1.0 edge_detector {
  } { 
    clk clk
    dout restart
  }

  # Configuration registers

  cell xilinx.com:ip:xlslice:1.0 reset_base_counter_slice {
    DIN_WIDTH 32
    DIN_FROM 0
    DIN_TO 0
  } {
    Din cfg
    Dout reset_base_counter/din
  }

  cell xilinx.com:ip:xlslice:1.0 start_slice {
    DIN_WIDTH 32
    DIN_FROM 1
    DIN_TO 1
  } {
    Din cfg
    Dout edge_detector/Din
  }

  cell xilinx.com:ip:c_shift_ram:12.0 delay_tvalid {
    Depth 1
    Width 1
  } {
    D reset_base_counter_slice/Dout
    CLK clk
    Q tvalid
  }

  current_bd_instance $bd
}

} ;# end address namespace
