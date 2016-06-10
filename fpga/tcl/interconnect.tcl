proc add_interconnect {module_name width n_inputs n_outputs} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  set sel_width [expr int(ceil(log($n_inputs)/log(2)))]

  create_bd_pin -dir I -type clk clk
  create_bd_pin -dir I clken
  create_bd_pin -dir I -from [expr $sel_width - 1] -to 0 sel

  for {set i 0} {$i < $n_inputs} {incr i} {
    create_bd_pin -dir I -from [expr $width - 1] -to 0 in$i
  }

  for {set j 0} {$j < $n_outputs} {incr j} {
    create_bd_pin -dir O -from [expr $width - 1] -to 0 out$j

    cell koheron:user:latched_mux:1.0 mux$j {
      WIDTH $width
      N_INPUTS $n_inputs
      SEL_WIDTH $sel_width
    } {
      clk clk
      clken clken
      out out$j
      sel sel
    }

    cell xilinx.com:ip:xlconcat:2.1 concat_$j {
      NUM_PORTS $n_inputs
    } {
      dout mux$j/in
    }

    for {set i 0} {$i < $n_inputs} {incr i} {
      set_property -dict [list CONFIG.IN${i}_WIDTH $width] [get_bd_cells concat_$j]
      connect_pins concat_$j/In$i in$i
    }

  }

  current_bd_instance $bd

}
