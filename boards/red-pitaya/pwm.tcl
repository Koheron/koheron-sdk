
proc add_pwm {module_name clk offset} {

	set bd [current_bd_instance .]
	current_bd_instance [create_bd_cell -type hier $module_name]

	create_bd_pin -dir I clk
  create_bd_pin -dir I -from 1023 -to 0 cfg

  connect_bd_net [get_bd_pins clk] [get_bd_pins /$clk]

	set n_pwm 4
	cell xilinx.com:ip:xlconcat:2.1 concat_pwm [list NUM_PORTS $n_pwm] {}
	connect_bd_net [get_bd_ports /dac_pwm_o] [get_bd_pins concat_pwm/dout]

	for {set i 0} {$i < $n_pwm} {incr i} {
		cell pavel-demin:user:pwm:1.0 pwm_$i {NBITS 10} \
		  [list clk clk rst /rst_ps_0_125M/peripheral_reset pwm_out concat_pwm/In$i]
		cell xilinx.com:ip:xlslice:1.0 pwm_slice_$i \
		  [list DIN_WIDTH 1024 DIN_FROM [expr 9+32*$i+$offset] DIN_TO [expr 32*$i+$offset]] \
		  [list Din cfg Dout pwm_$i/threshold]
	}

	current_bd_instance $bd

}
