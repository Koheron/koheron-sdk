
proc add_averager_float_module {module_name bram_addr_width} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I -type clk                             clk
  create_bd_pin -dir I                                       avg_off
  create_bd_pin -dir I                                       tvalid
  create_bd_pin -dir I                                       restart
  create_bd_pin -dir I -from 31                        -to 0 din
  create_bd_pin -dir O -from 31                        -to 0 dout
  create_bd_pin -dir O -from 3                         -to 0 wen
  create_bd_pin -dir O -from 31                        -to 0 count
  create_bd_pin -dir O -from 31                        -to 0 n_avg
  create_bd_pin -dir O -from 31                        -to 0 addr

  set add_latency 3
  set sr_latency 1
  set sr_avg_off_latency 1
  set fifo_rd_latency 1 

  # Create FIFO
  cell xilinx.com:ip:fifo_generator:13.0 fifo {
    Input_Data_Width 32
    Input_Depth      [expr 2**$bram_addr_width]
    Data_Count       true
    Data_Count_Width $bram_addr_width
    Reset_Pin        false
  } {
    clk clk
  }

  cell xilinx.com:ip:c_shift_ram:12.0 shift_reg_dout {
    Width.VALUE_SRC USER
    Width 32
    Depth 1
  } {
    CLK clk
    D   fifo/dout
    Q   dout
  }

  # Create Adder 
  cell xilinx.com:ip:floating_point:7.1 adder {
    A_Precision_Type.VALUE_SRC PROPAGATED
    Add_Sub_Value Add
    C_Optimization Low_Latency
    Flow_Control NonBlocking
    Maximum_Latency false
    C_Latency $add_latency
    C_Mult_Usage No_Usage
    C_Rate 1
  } {
    aclk clk
    s_axis_b_tdata din
    m_axis_result_tdata fifo/din
  }

  cell xilinx.com:ip:xlconstant:1.1 adder_valid {} {}
  connect_pins adder_valid/dout adder/s_axis_a_tvalid
  connect_pins adder_valid/dout adder/s_axis_b_tvalid

  # Connect tvalid to FIFO write enable
  cell xilinx.com:ip:c_shift_ram:12.0 wen_shift_reg {
    Width.VALUE_SRC USER
    Width 1
    Depth $add_latency
  } {
    CLK clk
    D   tvalid
    Q   fifo/wr_en
  }

  # Avg_off 

  cell xilinx.com:ip:c_shift_ram:12.0 sr_avg_off {
    Width.VALUE_SRC USER
    Width 32
    Depth $sr_avg_off_latency
    SCLR true
  } {
    CLK clk
    D fifo/dout
  }

  cell xilinx.com:ip:c_shift_ram:12.0 sr_avg_off_en {
    Width.VALUE_SRC USER
    Width 1
    Depth 1
    CE true
  } {
    CLK clk
    Q sr_avg_off/SCLR
    D avg_off
  }

  # Connect FIFO/dout to Adder (insert shift register)
  cell xilinx.com:ip:c_shift_ram:12.0 shift_reg {
    Width.VALUE_SRC USER
    Width 32
    Depth $sr_latency
    SCLR true
  } {
    CLK clk
    Q adder/s_axis_a_tdata
    D sr_avg_off/Q
  }

  # Enable reading FIFO once 
  # data_count == 2**$bram_addr_width - $add_latency - $sr_latency - $fifo_rd_latency)

  set threshold_val [expr 2**$bram_addr_width-$add_latency-$sr_latency-$sr_avg_off_latency-$fifo_rd_latency]

  cell pavel-demin:user:comparator:1.0 comp {
    DATA_WIDTH $bram_addr_width
  } {
    a       fifo/data_count
    a_geq_b fifo/rd_en
  }

  cell xilinx.com:ip:xlconstant:1.1 threshold {
    CONST_WIDTH $bram_addr_width
    CONST_VAL   $threshold_val
  } {
    dout comp/b
  } 

  # Start counting once FIFO read enabled

  cell xilinx.com:ip:c_counter_binary:12.0 counter {
    Output_Width 32
    CE true
    SCLR true
  } {
    CLK clk
    CE  comp/a_geq_b
  }

  cell xilinx.com:ip:c_shift_ram:12.0 shift_reg_counter {
    Width.VALUE_SRC USER
    Width 32
    Depth 1
  } {
    CLK clk
    Q count
    D counter/Q
  }

  # Number of averages

  cell xilinx.com:ip:xlslice:1.0 n_avg_slice {
    DOUT_WIDTH [expr 32-$bram_addr_width]
    DIN_FROM   [expr 32-1]
    DIN_TO     [expr $bram_addr_width]
  } {
    Din shift_reg_counter/Q
  }

  cell xilinx.com:ip:c_shift_ram:12.0 shift_reg_n_avg {
    Width.VALUE_SRC USER
    Width 32
    CE    true
    Depth 1
  } {
    CLK clk
    Q n_avg
    D n_avg_slice/Dout
  }

  # Write enable

  cell pavel-demin:user:write_enable:1.0 write_enable_0 {
    BRAM_WIDTH $bram_addr_width
  } {
    clk clk
    restart restart
    address counter/Q
    init counter/SCLR
  }

  connect_pins write_enable_0/init sr_avg_off_en/CE
  connect_pins write_enable_0/wen  shift_reg_n_avg/CE
  connect_pins write_enable_0/wen  shift_reg/SCLR

  cell xilinx.com:ip:xlconcat:2.1 concat_wen {NUM_PORTS 4} {dout wen}

  for {set i 0} {$i < 4} {incr i} {
    connect_pins concat_wen/In$i write_enable_0/wen
  }

  # Connect address

  cell xilinx.com:ip:xlconcat:2.1 concat_addr {
    NUM_PORTS 3
    IN0_WIDTH.VALUE_SRC USER IN0_WIDTH 2
    IN1_WIDTH.VALUE_SRC USER IN1_WIDTH $bram_addr_width
    IN2_WIDTH.VALUE_SRC USER IN2_WIDTH [expr 32-2-$bram_addr_width]
  } {
    In1 shift_reg_counter/Q
    dout addr
  }

  cell xilinx.com:ip:xlconstant:1.1 xlconstant_0 {
    CONST_WIDTH 2
    CONST_VAL   0
  } {
    dout concat_addr/In0
  }

  cell xilinx.com:ip:xlconstant:1.1 xlconstant_2 {
    CONST_WIDTH [expr 32-2-$bram_addr_width]
    CONST_VAL   0
  } {
    dout concat_addr/In2
  }


  current_bd_instance $bd

}
