
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

  cell xilinx.com:ip:xlconcat:2.1 concat_0 \
    [list IN0_WIDTH $adc_width IN1_WIDTH $adc_width] {}

  for {set i 1} {$i < 3} {incr i} {
    connect_pins subtract_$i/S concat_0/In[expr $i-1]
  }

  cell xilinx.com:ip:cmpy:6.0 complex_mult \
    [list \
      APortWidth 14 \
      BPortWidth 14 \
      OutputWidth 28] \
    [list \
      aclk clk \
      s_axis_a_tdata concat_0/dout]

  cell xilinx.com:ip:xlconstant:1.1 a_tvalid_const {} {dout complex_mult/s_axis_a_tvalid}
  cell xilinx.com:ip:xlconstant:1.1 b_tvalid_const {} {dout complex_mult/s_axis_b_tvalid}

  cell xilinx.com:ip:xfft:9.0 fft_0 \
    [list \
      transform_length 4096 \
      target_clock_frequency 125 \
      implementation_options pipelined_streaming_io \
      target_data_throughput 125 \
      data_format fixed_point \
      phase_factor_width 24 \
      throttle_scheme realtime \
      output_ordering natural_order] \
    [list \
      aclk clk \
      S_AXIS_DATA complex_mult/M_AXIS_DOUT]

  cell xilinx.com:ip:xlconstant:1.1 config_tvalid_const {} {dout fft_0/s_axis_config_tvalid}

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
