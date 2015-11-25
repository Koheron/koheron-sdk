
proc add_pwm {module_name clk offset pwm_width num_ports} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I clk
  
  for {set i 0} {$i < $num_ports} {incr i} {
    create_bd_pin -dir I -from 31 -to 0 pwm$i
  }

  create_bd_pin -dir I -from 1023 -to 0 cfg

  connect_bd_net [get_bd_pins clk] [get_bd_pins /$clk]

  cell xilinx.com:ip:xlconcat:2.1 concat_pwm [list NUM_PORTS $num_ports] {}
  connect_bd_net [get_bd_ports /dac_pwm_o] [get_bd_pins concat_pwm/dout]

  for {set i 0} {$i < $num_ports} {incr i} {
    cell pavel-demin:user:pwm:1.0 pwm_$i \
      [list NBITS $pwm_width] \
      [list clk clk rst /${::rst_name}/peripheral_reset pwm_out concat_pwm/In$i]
    cell xilinx.com:ip:xlslice:1.0 pwm_slice_$i \
      [list DIN_WIDTH 32 DIN_FROM [expr $pwm_width-1] DIN_TO 0] \
      [list Din pwm$i Dout pwm_$i/threshold]
  }

  current_bd_instance $bd

}
