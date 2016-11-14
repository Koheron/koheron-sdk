namespace eval peak_detector {

proc pins {cmd wfm_width} {
  $cmd -dir I -from 31                   -to 0 din
  $cmd -dir I -from [expr $wfm_width -1] -to 0 address_low
  $cmd -dir I -from [expr $wfm_width -1] -to 0 address_high
  $cmd -dir I -from [expr $wfm_width -1] -to 0 address_reset
  $cmd -dir I -from 0                    -to 0 s_axis_tvalid
  $cmd -dir O -from [expr $wfm_width -1] -to 0 address_out
  $cmd -dir O -from 31                   -to 0 maximum_out
  $cmd -dir O -from 0                    -to 0 m_axis_tvalid
  $cmd -dir I -type clk                        clk
}

proc create {module_name wfm_width} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  pins create_bd_pin $wfm_width

  set compare_latency 0

# Add comparator
  cell xilinx.com:ip:floating_point:7.1 comparator {
    Operation_Type Compare
    C_Compare_Operation Greater_Than
    Flow_Control NonBlocking
    Maximum_Latency False
    C_Latency $compare_latency
  } {
    s_axis_a_tdata din
    s_axis_a_tvalid s_axis_tvalid
    s_axis_b_tvalid s_axis_tvalid
  }

  cell xilinx.com:ip:xlslice:1.0 slice_compare {
    DIN_WIDTH 8
  } {
    Din comparator/m_axis_result_tdata
  }

  # Address starting counting at s_axis_tvalid
  cell xilinx.com:ip:c_counter_binary:12.0 address_counter {
    CE true
    Output_Width $wfm_width
  } {
    CLK clk
    CE s_axis_tvalid
  }

  cell koheron:user:comparator:1.0 reset_cycle {
    DATA_WIDTH $wfm_width
    OPERATION "EQ"
  } {
    a address_counter/Q
    b address_reset
  }

  # OR
  cell xilinx.com:ip:util_vector_logic:2.0 logic_or {
    C_SIZE 1
    C_OPERATION or
  } {
    Op2 reset_cycle/dout
  }

  # Register storing the current maximum
  cell xilinx.com:ip:c_shift_ram:12.0 maximum_reg {
    CE true
    Width 32
    Depth 1
  } {
    CLK clk
    D din
    CE logic_or/Res
    Q comparator/s_axis_b_tdata
  }

  # Register storing the address of current maximum
  cell xilinx.com:ip:c_shift_ram:12.0 address_reg {
    CE true
    Width $wfm_width
    Depth 1
  } {
    CLK clk
    D address_counter/Q
    CE logic_or/Res
  }

  # Register storing the maximum of one cycle
  cell xilinx.com:ip:c_shift_ram:12.0 maximum_out {
    CE true
    Width 32
    Depth 1
  } {
    CLK clk
    CE reset_cycle/dout
    D maximum_reg/Q
    Q maximum_out
  }

  # Register storing the address of the maximum of one cycle
  cell xilinx.com:ip:c_shift_ram:12.0 address_out {
    CE true
    Width $wfm_width
    Depth 1
  } {
    CLK clk
    CE reset_cycle/Dout
    D address_reg/Q
    Q address_out
  }

  # Restrict peak detection between address_low and address_high

  cell koheron:user:comparator:1.0 address_ge_low {
    DATA_WIDTH $wfm_width
    OPERATION "GE"  
  } {
    a address_counter/Q
    b address_low
  }

  cell koheron:user:comparator:1.0 address_le_high {
    DATA_WIDTH $wfm_width
    OPERATION "LE"  
  } {
    a address_counter/Q
    b address_high
  }

  cell xilinx.com:ip:util_vector_logic:2.0 address_in_range {
    C_SIZE 1
    C_OPERATION and
  } {
    Op1 address_ge_low/dout
    Op2 address_le_high/dout
  }

  cell xilinx.com:ip:util_vector_logic:2.0 maximum_detected_in_range {
    C_SIZE 1
    C_OPERATION and
  } {
    Op1 address_in_range/Res
    Op2 slice_compare/dout
    Res logic_or/Op1
  }

  # Register storing the current maximum
  cell xilinx.com:ip:c_shift_ram:12.0 shift_tvalid {
    Width 32
    Depth 1
  } {
    CLK clk
    D reset_cycle/dout
    Q m_axis_tvalid
  }

  current_bd_instance $bd

}

} ;# end peak detector namespace
