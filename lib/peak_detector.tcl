
proc add_peak_detector {module_name wfm_width} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I -type clk              clk
  create_bd_pin -dir I -from 31 -to 0         din
  create_bd_pin -dir I                        tvalid
  create_bd_pin -dir O -from $wfm_width -to 0 address_out
  create_bd_pin -dir O -from 31 -to 0         maximum_out
  set compare_latency 2

  # Add comparator
  cell xilinx.com:ip:floating_point:7.1 comparator {
    Operation_Type Compare
    C_Compare_Operation Greater_Than
    Flow_Control NonBlocking
    Maximum_Latency False
    C_Latency $compare_latency
  } {
    aclk clk
    s_axis_a_tdata din
    s_axis_a_tvalid tvalid
    s_axis_b_tvalid tvalid
  }

  cell xilinx.com:ip:xlslice:1.0 slice_compare {
    DIN_WIDTH 8
  } {
    Din comparator/m_axis_result_tdata
  }

  cell xilinx.com:ip:c_shift_ram:12.0 shift_data {
    Width 32
    Depth $compare_latency
  } {
    CLK clk
    D din
  }

  # 
  cell xilinx.com:ip:c_shift_ram:12.0 shift_reg_maximum {
    CE true
    Width 32
    Depth 1
    SCLR true
  } {
    CLK clk
    D shift_data/Q
    CE slice_compare/Dout
    Q comparator/s_axis_b_tdata
  }

  cell xilinx.com:ip:c_counter_binary:12.0 address_counter {
    CE true
    Output_Width $wfm_width
  } {
    CLK clk
    CE tvalid
  }

  cell xilinx.com:ip:c_shift_ram:12.0 shift_address {
    Width $wfm_width
    Depth 1
  } {
    CLK clk
    D address_counter/Q
  }

  cell xilinx.com:ip:c_shift_ram:12.0 shift_reg_address {
    CE true
    Width $wfm_width
    Depth 1
    SCLR true
  } {
    CLK clk
    CE slice_compare/Dout
    D shift_address/Q
  }

  cell xilinx.com:ip:xlconstant:1.1 start_cycle_constant {
    CONST_WIDTH $wfm_width
    CONST_VAL 0
  } {}

  cell koheron:user:comparator:1.0 start_cycle {
    DATA_WIDTH $wfm_width
    OPERATION "EQ"
  } {
    a address_counter/Q
    b start_cycle_constant/dout
    dout shift_reg_maximum/SCLR
    dout shift_reg_address/SCLR
  }

  cell xilinx.com:ip:c_shift_ram:12.0 shift_reg_address_out {
    CE true
    Width $wfm_width
    Depth 1
  } {
    CLK clk
    CE start_cycle/dout
    D shift_reg_address/Q
    Q address_out
  }

  cell xilinx.com:ip:c_shift_ram:12.0 shift_reg_maximum_out {
    CE true
    Width 32
    Depth 1
  } {
    CLK clk
    CE start_cycle/dout
    D shift_reg_maximum/Q
    Q maximum_out
  }


  current_bd_instance $bd

}
