
proc add_spectrum_module {module_name bram_addr_width adc_width clk} {

	set bd [current_bd_instance .]
	current_bd_instance [create_bd_cell -type hier $module_name]

	create_bd_pin -dir I                                   clk
	create_bd_pin -dir I -from [expr $adc_width - 1] -to 0 adc1
	create_bd_pin -dir I -from [expr $adc_width - 1] -to 0 adc2
	create_bd_pin -dir I -from 31                    -to 0 cfg
	create_bd_pin -dir O -from 31                    -to 0 psd

  connect_pins clk /$clk

  for {set i 1} {$i < 3} {incr i} {

	  cell xilinx.com:ip:c_addsub:12.0 subtract_$i \
	    [list                                       \
	      A_Width   $adc_width                      \
	      B_Width   $adc_width                      \
	      Add_mode  Subtract                        \
	      CE        false                           \
	      Out_Width $adc_width]                     \
	    [list                                       \
	      A   adc$i                                 \
	      clk clk]
  }

  # Configuration registers

  for {set i 1} {$i < 3} {incr i} {
    cell xilinx.com:ip:xlslice:1.0 subtract_slice_$i \
      [list                                          \
        DIN_WIDTH 32                                 \
        DIN_FROM  [expr $adc_width*$i-1]             \
        DIN_TO    [expr $adc_width*($i-1)]]          \
      [list Din cfg Dout subtract_$i/B]
  }


	current_bd_instance $bd

}
