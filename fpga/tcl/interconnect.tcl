proc add_interconnect {module_name width n_inputs n_outputs} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  set sel_width [expr int(ceil(log($n_inputs)/log(2)))]

  create_bd_pin -dir I -type clk clk
  create_bd_pin -dir I -from 0                                  -to 0 clken
  create_bd_pin -dir I -from [expr $sel_width * $n_outputs - 1] -to 0 sel

  for {set i 0} {$i < $n_inputs} {incr i} {
    create_bd_pin -dir I -from [expr $width - 1] -to 0 in$i
  }

  set concat_input [get_concat_pin [lmap pin [range 0 $n_inputs] {set pin in$pin}]]

  for {set j 0} {$j < $n_outputs} {incr j} {
    create_bd_pin -dir O -from [expr $width - 1] -to 0 out$j

    set from  [expr ($j + 1) * $sel_width - 1]
    set to    [expr $j * $sel_width]

    cell koheron:user:latched_mux:1.0 mux$j {
      N_INPUTS $n_inputs
      SEL_WIDTH $sel_width
    } {
      clk   clk
      clken clken
      out   out$j
      sel   [get_slice_pin sel $from $to]
      in    $concat_input
    }

  }

  current_bd_instance $bd

}
