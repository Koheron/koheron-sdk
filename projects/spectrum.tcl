
proc add_spectrum_module {module_name n_pts_fft adc_width clk} {

	set bd [current_bd_instance .]
	current_bd_instance [create_bd_cell -type hier $module_name]

	create_bd_pin -dir I -type clk                         clk
	create_bd_pin -dir I -from [expr $adc_width - 1] -to 0 adc1
	create_bd_pin -dir I -from [expr $adc_width - 1] -to 0 adc2
	create_bd_pin -dir I -from 31                    -to 0 cfg_sub
  create_bd_pin -dir I -from 31                    -to 0 cfg_fft

  create_bd_intf_pin -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 MAXIS_RESULT

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
      transform_length $n_pts_fft \
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

  for {set i 0} {$i < 2} {incr i} {
    cell xilinx.com:ip:xlslice:1.0 fft_slice_$i \
      [list                                     \
        DIN_WIDTH 64                            \
        DIN_FROM  [expr 31+32*$i]               \
        DIN_TO    [expr 32*$i]]                 \
      [list Din fft_0/m_axis_data_tdata]

    cell xilinx.com:ip:floating_point:7.1 mult_$i \
      [list                                       \
        Operation_Type Multiply                   \
        Flow_Control NonBlocking ]                \
      [list                                       \
        aclk clk                                  \
        s_axis_a_tdata fft_slice_$i/Dout          \
        s_axis_b_tdata fft_slice_$i/Dout          \
        s_axis_a_tvalid fft_0/m_axis_data_tvalid  \
        s_axis_b_tvalid fft_0/m_axis_data_tvalid  \
      ]
  }

  cell xilinx.com:ip:floating_point:7.1 floating_point_0 \
    [list                                                \
      Flow_Control NonBlocking                           \
      Add_Sub_Value Add]                                 \
    [list                                                \
      aclk clk                                           \
      S_AXIS_A mult_0/M_AXIS_RESULT                      \
      S_AXIS_B mult_1/M_AXIS_RESULT                      \
    ]

  connect_bd_intf_net [get_bd_intf_pins MAXIS_RESULT] [get_bd_intf_pins floating_point_0/M_AXIS_RESULT]

  # Configuration registers

  for {set i 1} {$i < 3} {incr i} {
    cell xilinx.com:ip:xlslice:1.0 subtract_slice_$i \
      [list                                          \
        DIN_WIDTH 32                                 \
        DIN_FROM  [expr $adc_width*$i-1]             \
        DIN_TO    [expr $adc_width*($i-1)]]          \
      [list Din cfg_sub Dout subtract_$i/B]
  }

  cell xilinx.com:ip:xlslice:1.0 subtract_slice_$i \
    [list DIN_WIDTH 32 DIN_FROM 15 DIN_TO 0]       \
    [list Din cfg_fft Dout fft_0/s_axis_config_tdata]

	current_bd_instance $bd

}
