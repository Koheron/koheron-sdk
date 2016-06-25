namespace eval spectrum {

proc pins {cmd adc_width} {
  $cmd -dir I -from [expr $adc_width - 1] -to 0 adc1
  $cmd -dir I -from [expr $adc_width - 1] -to 0 adc2
  $cmd -dir I -from 31                    -to 0 cfg_sub
  $cmd -dir I -from 31                    -to 0 cfg_fft
  $cmd -dir I -from 31                    -to 0 demod_data
  $cmd -dir I                                   tvalid
  $cmd -dir O -from 31                    -to 0 m_axis_result_tdata
  $cmd -dir O                                   m_axis_result_tvalid
  $cmd -dir I -type clk                         clk
}

proc create {module_name n_pts_fft adc_width} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  pins create_bd_pin $adc_width

  for {set i 1} {$i < 3} {incr i} {
    cell xilinx.com:ip:c_addsub:12.0 subtract_$i {
      A_Width   $adc_width
      B_Width   $adc_width
      Add_mode  Subtract
      CE        false
      Out_Width $adc_width
      Latency 2
    } {
      clk clk
      A   adc$i
      B   [get_slice_pin cfg_sub 32 [expr $adc_width*$i-1] [expr $adc_width*($i-1)]]
    }
  }

  set left_width [expr 16 - $adc_width]
  cell xilinx.com:ip:xlconcat:2.1 concat_0 {
    NUM_PORTS 4
    IN0_WIDTH $adc_width
    IN1_WIDTH $left_width
    IN2_WIDTH $adc_width
    IN3_WIDTH $left_width
  } {
    In1 [get_constant_pin 0 $left_width]
    In3 [get_constant_pin 0 $left_width]
  }

  for {set i 1} {$i < 3} {incr i} {
    connect_pins subtract_$i/S concat_0/In[expr 2*($i-1)]
  }

  cell xilinx.com:ip:cmpy:6.0 complex_mult {
    APortWidth $adc_width
    BPortWidth $adc_width
    OutputWidth [expr 2*$adc_width + 1]
  } {
    aclk clk
    s_axis_a_tdata concat_0/dout
    s_axis_b_tdata demod_data
    s_axis_a_tvalid [get_Q_pin tvalid 1 2]
    s_axis_b_tvalid [get_Q_pin tvalid 1 2]
  }
  
  for {set i 0} {$i < 2} {incr i} {
    cell xilinx.com:ip:floating_point:7.1 float_$i {
      Operation_Type Fixed_to_float
      A_Precision_Type Custom
      C_A_Exponent_Width [expr 2*$adc_width + 1]
      Flow_Control NonBlocking
      Maximum_Latency False
      C_Latency 2
    } {
      aclk clk
      s_axis_a_tdata [get_slice_pin complex_mult/m_axis_dout_tdata 64 [expr 31+32*$i] [expr 32*$i]]
      s_axis_a_tvalid complex_mult/m_axis_dout_tvalid
    }
  }

  cell xilinx.com:ip:xlconcat:2.1 concat_float {
    IN0_WIDTH 32
    IN1_WIDTH 32
  } {
    In0 float_0/m_axis_result_tdata
    In1 float_1/m_axis_result_tdata
  }

  cell xilinx.com:ip:xfft:9.0 fft_0 {
    transform_length $n_pts_fft
    target_clock_frequency 125
    implementation_options pipelined_streaming_io
    data_format floating_point
    phase_factor_width 24
    throttle_scheme realtime
    output_ordering natural_order
  } {
    aclk clk
    s_axis_data_tdata    concat_float/dout
    s_axis_data_tvalid   [get_and_pin float_0/m_axis_result_tvalid float_1/m_axis_result_tvalid]
    s_axis_data_tlast    [get_constant_pin 0 1]
    s_axis_config_tdata  [get_slice_pin cfg_fft 32 15 0]
    s_axis_config_tvalid [get_constant_pin 1 1]
  }

  for {set i 0} {$i < 2} {incr i} {
  
    set slice_tdata [get_slice_pin fft_0/m_axis_data_tdata 64 [expr 31+32*$i] [expr 32*$i]]
    cell xilinx.com:ip:floating_point:7.1 mult_$i {
      Operation_Type Multiply
      Flow_Control NonBlocking
      Maximum_Latency False
      C_Latency 3
    } {
      aclk clk
      s_axis_a_tdata  $slice_tdata
      s_axis_b_tdata  $slice_tdata
      s_axis_a_tvalid fft_0/m_axis_data_tvalid
      s_axis_b_tvalid fft_0/m_axis_data_tvalid
    }
  }

  cell xilinx.com:ip:floating_point:7.1 add_0 {
    Flow_Control NonBlocking
    Add_Sub_Value Add
    C_Mult_Usage No_Usage
    Maximum_Latency False
    C_Latency 3
  } {
    aclk clk
    S_AXIS_A mult_0/M_AXIS_RESULT
    S_AXIS_B mult_1/M_AXIS_RESULT
    m_axis_result_tdata m_axis_result_tdata
    m_axis_result_tvalid m_axis_result_tvalid
  }

  current_bd_instance $bd
}

} ;# end spectrum namespace
