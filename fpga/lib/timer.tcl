namespace eval timer {

proc pins {cmd} {
  $cmd -dir I -type clk clk
  $cmd -dir I -from 31 -to 0 cfg
  $cmd -dir I -from 0  -to 0 clken_in
  $cmd -dir O -from 0  -to 0 clken_out  
  $cmd -dir O -from 31 -to 0 counter0
  $cmd -dir O -from 31 -to 0 counter1
}

proc create {module_name} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]
  pins create_bd_pin

  set clken \
    [get_or_pin \
      [get_slice_pin cfg 1 1] \
      [get_and_pin \
        [get_slice_pin cfg 0 0] \
        clken_in]]

  cell xilinx.com:ip:c_counter_binary:12.0 counter {Output_Width 64} {CLK clk}
  
  set counter  [get_Q_pin counter/Q 1 $clken]

  connect_pins clken_out $clken
  connect_pins counter0  [get_slice_pin $counter 31 0]
  connect_pins counter1  [get_slice_pin $counter 63 32]

  current_bd_instance $bd
}

} ;# end timer namespace
