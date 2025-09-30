namespace eval power_spectral_density {

proc pins {cmd} {
  $cmd -dir I -from 31  -to 0 data
  $cmd -dir I -from 31  -to 0 ctl_fft
  $cmd -dir I -from 0   -to 0 tvalid
  $cmd -dir O -from 31  -to 0 m_axis_result_tdata
  $cmd -dir O -from 0   -to 0 m_axis_result_tvalid
  $cmd -dir I -type clk       clk
}

proc create {module_name fft_size} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  pins create_bd_pin

  cell xilinx.com:ip:c_counter_binary:12.0 demod_address {
    CE true
    Output_Width [expr int(log([get_parameter fft_size])/log(2)) + 2]
    Increment_Value 4
  } {
    CLK clk
    CE tvalid
  }

  # Add demod BRAM
  set demod_bram_name [add_bram demod 0]
  connect_cell $demod_bram_name {
    clkb  clk
    addrb demod_address/Q
    dinb  [get_constant_pin 0 32]
    enb   [get_Q_pin tvalid 1]
    web   [get_constant_pin 0 4]
  }

  set shifted_tvalid [get_Q_pin tvalid 3]

  cell xilinx.com:ip:cmpy:6.0 complex_mult {
    APortWidth 19
    BPortWidth 16
    OutputWidth 36
    LatencyConfig Manual
    MinimumLatency 7
  } {
    aclk            clk
    s_axis_a_tdata  [get_concat_pin [list [get_Q_pin data 3] [get_constant_pin 0 24]]]
    s_axis_b_tdata  [get_Q_pin $demod_bram_name/doutb 1]
    s_axis_a_tvalid $shifted_tvalid
    s_axis_b_tvalid $shifted_tvalid
  }

  cell xilinx.com:ip:axis_data_fifo:2.0 fifo {
    FIFO_DEPTH $fft_size
    TDATA_NUM_BYTES 10
  } {
    S_AXIS complex_mult/M_AXIS_DOUT
    s_axis_aclk s_axi_aclk
    s_axis_aresetn s_axi_aresetn
  }

  for {set i 0} {$i < 2} {incr i} {
    cell xilinx.com:ip:floating_point:7.1 float_$i {
      Operation_Type Fixed_to_float
      A_Precision_Type Custom
      C_A_Exponent_Width 19
      C_A_Fraction_Width 17
      Flow_Control Blocking
      Maximum_Latency True
    } {
      aclk clk
      s_axis_a_tdata [get_slice_pin fifo/m_axis_tdata [expr 35+40*$i] [expr 40*$i]]
      s_axis_a_tvalid fifo/m_axis_tvalid
    }
  }

  connect_pins float_0/s_axis_a_tready fifo/m_axis_tready

  cell xilinx.com:ip:xfft:9.0 fft_0 {
    transform_length $fft_size
    implementation_options automatically_select
    data_format floating_point
    target_clock_frequency [expr [get_parameter fclk0] / 1000000]
    target_data_throughput 20
    phase_factor_width 25
    throttle_scheme nonrealtime
    output_ordering natural_order
    complex_mult_type use_mults_resources
    number_of_stages_using_block_ram_for_data_and_phase_factors 5
    butterfly_type use_xtremedsp_slices
  } {
    aclk clk
    s_axis_data_tdata    [get_concat_pin {
                           float_0/m_axis_result_tdata
                           float_1/m_axis_result_tdata
                         }]
    s_axis_data_tvalid   [get_and_pin float_0/m_axis_result_tvalid float_1/m_axis_result_tvalid]
    s_axis_data_tlast    [get_constant_pin 0 1]
    s_axis_config_tdata  ctl_fft
    s_axis_config_tvalid [get_constant_pin 1 1]
  }

  connect_pins float_0/m_axis_result_tready fft_0/s_axis_data_tready
  connect_pins float_1/m_axis_result_tready fft_0/s_axis_data_tready

  for {set i 0} {$i < 2} {incr i} {
    set slice_tdata [get_slice_pin fft_0/m_axis_data_tdata [expr 31+32*$i] [expr 32*$i]]
    cell xilinx.com:ip:floating_point:7.1 mult_$i {
      Operation_Type Multiply
      Flow_Control Blocking
      Maximum_Latency True
    } {
      aclk clk
      s_axis_a_tdata  $slice_tdata
      s_axis_b_tdata  $slice_tdata
      s_axis_a_tvalid fft_0/m_axis_data_tvalid
      s_axis_b_tvalid fft_0/m_axis_data_tvalid
    }
  }

  connect_pins fft_0/m_axis_data_tready mult_0/s_axis_a_tready

  cell xilinx.com:ip:floating_point:7.1 add_0 {
    Flow_Control Blocking
    Add_Sub_Value Add
    C_Mult_Usage No_Usage
    Maximum_Latency True
    Has_RESULT_TREADY false
  } {
    aclk clk
    S_AXIS_A mult_0/M_AXIS_RESULT
    S_AXIS_B mult_1/M_AXIS_RESULT
    m_axis_result_tdata m_axis_result_tdata
    m_axis_result_tvalid m_axis_result_tvalid
  }

  current_bd_instance $bd
}

} ;# end power_spectral_density namespace