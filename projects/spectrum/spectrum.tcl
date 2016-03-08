
proc add_spectrum_module {module_name n_pts_fft adc_width clk} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I -type clk                         clk
  create_bd_pin -dir I -from [expr $adc_width - 1] -to 0 adc1
  create_bd_pin -dir I -from [expr $adc_width - 1] -to 0 adc2
  create_bd_pin -dir I -from 31                    -to 0 cfg_sub
  create_bd_pin -dir I -from 31                    -to 0 cfg_fft
  create_bd_pin -dir I -from 31                    -to 0 demod_data
  create_bd_pin -dir I                                   tvalid
  create_bd_pin -dir O -from 31                    -to 0 m_axis_result_tdata
  create_bd_pin -dir O                                   m_axis_result_tvalid

  #create_bd_intf_pin -mode Master -vlnv xilinx.com:interface:axis_rtl:1.0 MAXIS_RESULT

  connect_pins clk /$clk

  for {set i 1} {$i < 3} {incr i} {
    cell xilinx.com:ip:c_addsub:12.0 subtract_$i {
      A_Width   $adc_width
      B_Width   $adc_width
      Add_mode  Subtract
      CE        false
      Out_Width $adc_width
      Latency 2
    } {
      A   adc$i
      clk clk
    }
  }

  cell xilinx.com:ip:xlconcat:2.1 concat_0 {
    IN0_WIDTH $adc_width
    IN1_WIDTH $adc_width
  } {}

  for {set i 1} {$i < 3} {incr i} {
    connect_pins subtract_$i/S concat_0/In[expr $i-1]
  }

  cell xilinx.com:ip:cmpy:6.0 complex_mult {
    APortWidth $adc_width
    BPortWidth $adc_width
    OutputWidth [expr 2*$adc_width + 1]
  } {
    aclk clk
    s_axis_a_tdata concat_0/dout
    s_axis_b_tdata demod_data
  }

  for {set i 0} {$i < 2} {incr i} {
    cell xilinx.com:ip:xlslice:1.0 mult_slice_$i {
      DIN_WIDTH 64
      DIN_FROM  [expr 31+32*$i]
      DIN_TO    [expr 32*$i]
    } {
      Din complex_mult/m_axis_dout_tdata
    }
    cell xilinx.com:ip:floating_point:7.1 float_$i {
      Operation_Type Fixed_to_float
      A_Precision_Type Custom
      C_A_Exponent_Width [expr 2*$adc_width + 1]
      Flow_Control NonBlocking
    } {
      aclk clk
      s_axis_a_tdata mult_slice_$i/dout
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

  cell xilinx.com:ip:util_vector_logic:2.0 tvalid_and {
    C_SIZE 1
    C_OPERATION and
  } {
    Op1 float_0/m_axis_result_tvalid
    Op2 float_1/m_axis_result_tvalid
  }

  cell xilinx.com:ip:c_shift_ram:12.0 shift_tvalid {
    Width 1
    Depth 2
  } {
    CLK clk
    D tvalid
  }

  connect_pins shift_tvalid/Q complex_mult/s_axis_a_tvalid
  connect_pins shift_tvalid/Q complex_mult/s_axis_b_tvalid

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
    s_axis_data_tdata concat_float/dout
    s_axis_data_tvalid tvalid_and/Res
  }

  cell xilinx.com:ip:xlconstant:1.1 config_tlast_const {CONST_VAL 0} {dout fft_0/s_axis_data_tlast}
  cell xilinx.com:ip:xlconstant:1.1 config_tvalid_const {} {dout fft_0/s_axis_config_tvalid}

  for {set i 0} {$i < 2} {incr i} {
    cell xilinx.com:ip:xlslice:1.0 fft_slice_$i {
      DIN_WIDTH 64
      DIN_FROM  [expr 31+32*$i]
      DIN_TO    [expr 32*$i]
    } {
      Din fft_0/m_axis_data_tdata
    }

    cell xilinx.com:ip:floating_point:7.1 mult_$i {
      Operation_Type Multiply
      Flow_Control NonBlocking
    } {
      aclk clk
      s_axis_a_tdata fft_slice_$i/Dout
      s_axis_b_tdata fft_slice_$i/Dout
      s_axis_a_tvalid fft_0/m_axis_data_tvalid
      s_axis_b_tvalid fft_0/m_axis_data_tvalid
    }
  }

  cell xilinx.com:ip:floating_point:7.1 add_0 {
    Flow_Control NonBlocking
    Add_Sub_Value Add
    C_Mult_Usage No_Usage
  } {
    aclk clk
    S_AXIS_A mult_0/M_AXIS_RESULT
    S_AXIS_B mult_1/M_AXIS_RESULT
    m_axis_result_tdata m_axis_result_tdata
    m_axis_result_tvalid m_axis_result_tvalid
  }

  # Configuration registers

  for {set i 1} {$i < 3} {incr i} {
    cell xilinx.com:ip:xlslice:1.0 subtract_slice_$i {
      DIN_WIDTH 32
      DIN_FROM  [expr $adc_width*$i-1]
      DIN_TO    [expr $adc_width*($i-1)]
    } {
      Din cfg_sub
      Dout subtract_$i/B
    }
  }

  cell xilinx.com:ip:xlslice:1.0 cfg_fft_slice {
    DIN_WIDTH 32
    DIN_FROM 15
    DIN_TO 0
  } {
    Din cfg_fft
    Dout fft_0/s_axis_config_tdata
  }

  current_bd_instance $bd

}
