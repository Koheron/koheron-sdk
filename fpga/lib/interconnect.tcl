namespace eval interconnect {

proc pins {cmd width n_inputs n_outputs} {
  $cmd -dir I -type clk clk
  $cmd -dir I -from 0 -to 0 clken
  $cmd -dir I -from [expr [get_sel_width $n_inputs] * $n_outputs - 1] -to 0 sel

  for {set i 0} {$i < $n_inputs} {incr i} {
    $cmd -dir I -from [expr $width - 1] -to 0 din$i
  }

  for {set j 0} {$j < $n_outputs} {incr j} {
    $cmd -dir O -from [expr $width - 1] -to 0 dout$j
  }
}

proc get_sel_width {n_inputs} {
  return [expr int(ceil(log($n_inputs)/log(2)))]
}

proc create {module_name width n_inputs n_outputs} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  pins create_bd_pin $width $n_inputs $n_outputs

  set sel_width [get_sel_width $n_inputs]
  set concat_input [get_concat_pin [lmap pin [range 0 $n_inputs] {set pin din$pin}]]

  for {set j 0} {$j < $n_outputs} {incr j} {

    set from  [expr ($j + 1) * $sel_width - 1]
    set to    [expr $j * $sel_width]

    cell koheron:user:latched_mux:1.0 mux$j {
      N_INPUTS $n_inputs
      SEL_WIDTH $sel_width
      WIDTH $width
    } {
      clk   clk
      clken clken
      dout  dout$j
      sel   [get_slice_pin sel $from $to]
      din   $concat_input
    }

  }

  current_bd_instance $bd

}
} ;# end interconnect namespace
