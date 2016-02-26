
proc add_pwm {module_name clk offset pwm_width num_ports {output_port dac_pwm_o}} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I clk
  create_bd_pin -dir I rst
  
  for {set i 0} {$i < $num_ports} {incr i} {
    create_bd_pin -dir I -from 31 -to 0 pwm$i
  }

  connect_pins clk /$clk

  cell xilinx.com:ip:xlconcat:2.1 concat_pwm {NUM_PORTS $num_ports} {}
  connect_bd_net [get_bd_ports /$output_port] [get_bd_pins concat_pwm/dout]

  for {set i 0} {$i < $num_ports} {incr i} {
    cell koheron:user:pwm:1.0 pwm_$i {
      NBITS $pwm_width
    } {
      clk clk
      rst rst
      pwm_out concat_pwm/In$i
    }
    cell xilinx.com:ip:xlslice:1.0 pwm_slice_$i {
      DIN_WIDTH 32
      DIN_FROM [expr $pwm_width-1]
      DIN_TO 0
    } {
      Din pwm$i
      Dout pwm_$i/threshold
    }
  }

  current_bd_instance $bd

}
