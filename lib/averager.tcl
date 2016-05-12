
proc add_averager_module {module_name bram_addr_width args} {

  # Input type
  # examples: float, fix_14
  array set optional [list -input_type "float" {*}$args]
  set input_type $optional(-input_type)

  puts "Input type = $input_type"

  if { [string match "fix_*" $input_type] } {
    set type fix
    scan $input_type {fix_%d} width
    puts "type == fix"
  } else {
    set type float
    set width 32
    puts "type == float"
  }

  set fast_count_width $bram_addr_width
  set slow_count_width [expr 32 - $bram_addr_width]

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I -type clk                              clk
  create_bd_pin -dir I                                        avg_off
  create_bd_pin -dir I                                        tvalid
  create_bd_pin -dir I                                        restart
  create_bd_pin -dir I -from [expr $fast_count_width-1] -to 0 period
  create_bd_pin -dir I -from [expr $slow_count_width-1] -to 0 threshold
  create_bd_pin -dir I -from [expr $width-1]            -to 0 din
  create_bd_pin -dir O -from 31                         -to 0 dout
  create_bd_pin -dir O -from 3                          -to 0 wen
  create_bd_pin -dir O -from [expr $slow_count_width-1] -to 0 n_avg
  create_bd_pin -dir O -from 31                         -to 0 addr
  create_bd_pin -dir O                                        ready
 
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
    dout dout
  }

  # Create Adder (depends on input type)
  if { $type == "fix" } {	  
    cell xilinx.com:ip:c_addsub:12.0 adder {
      A_Width.VALUE_SRC USER
      B_Width.VALUE_SRC USER
      A_Width           32
      B_Width           $width
      Out_Width         32
      CE                false
      Latency           $add_latency
    } {
      CLK clk
      B   din
      S   fifo/din
    }
    set adder_A {adder/A}
  } else {
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
    set adder_A {adder/s_axis_a_tdata}

    cell xilinx.com:ip:xlconstant:1.1 adder_valid {} {}
    connect_pins adder_valid/dout adder/s_axis_a_tvalid
    connect_pins adder_valid/dout adder/s_axis_b_tvalid

  }

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
    Q $adder_A
    D sr_avg_off/Q
  }

  # Enable reading FIFO once 
  # data_count == 2**$bram_addr_width - $add_latency - $sr_latency - $fifo_rd_latency)

  cell koheron:user:comparator:1.0 comp {
    DATA_WIDTH $bram_addr_width
    OPERATION "GE"
  } {
    a fifo/data_count
    b threshold
    dout fifo/rd_en
  }

  # Start counting once FIFO read enabled

  cell koheron:user:averager_counter:1.0 averager_counter {
    FAST_COUNT_WIDTH $fast_count_width
    SLOW_COUNT_WIDTH $slow_count_width
  } {
    clk clk
    clken comp/dout
    count_max period
    restart restart
    n_avg n_avg
    init sr_avg_off_en/CE
    ready ready
    wen shift_reg/SCLR
    address addr
  }

  cell xilinx.com:ip:xlconcat:2.1 concat_wen {NUM_PORTS 4} {dout wen}

  for {set i 0} {$i < 4} {incr i} {
    connect_pins concat_wen/In$i averager_counter/wen
  }

  current_bd_instance $bd

}
