namespace eval averager {

proc pins {cmd fast_count_width slow_count_width width} {
   $cmd -dir I -from 0                          -to 0 avg_on
   $cmd -dir I -from 0                          -to 0 tvalid
   $cmd -dir I -from 0                          -to 0 restart
   $cmd -dir I -from [expr $fast_count_width-1] -to 0 period
   $cmd -dir I -from [expr $fast_count_width-1] -to 0 threshold
   $cmd -dir I -from [expr $slow_count_width-1] -to 0 n_avg_min
   $cmd -dir I -from [expr $width-1]            -to 0 din
   $cmd -dir O -from 31                         -to 0 dout
   $cmd -dir O -from 3                          -to 0 wen
   $cmd -dir O -from [expr $slow_count_width-1] -to 0 n_avg
   $cmd -dir O -from 31                         -to 0 addr
   $cmd -dir O -from 0                          -to 0 ready
   $cmd -dir O -from 0                          -to 0 avg_on_out
   $cmd -dir O -from 0                          -to 0 new_cycle
   $cmd -dir I -type clk                              clk
}

proc create {module_name bram_addr_width args} {

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

  pins create_bd_pin $fast_count_width $slow_count_width $width

  set add_latency 3
  set sr_latency 1
  set sr_avg_off_latency 1
  set fifo_rd_latency 1

  # Create FIFO
  cell xilinx.com:ip:fifo_generator:13.1 fifo {
    Input_Data_Width 32
    Input_Depth      [expr 2**$bram_addr_width]
    Data_Count       true
    Data_Count_Width $bram_addr_width
    Reset_Pin        true
  } {
    clk clk
    srst [get_not_pin tvalid]
  }

  connect_pins dout [get_Q_pin fifo/dout 1]

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
  connect_pins fifo/wr_en [get_Q_pin tvalid $add_latency]

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

  # Start counting once FIFO read enabled
  set clken [get_and_pin \
              [get_GE_pin \
                fifo/data_count \
                [get_Q_pin threshold 1 [get_not_pin tvalid]]] \
              [get_Q_pin tvalid $add_latency]]
  connect_pins $clken fifo/rd_en

  cell koheron:user:averager_counter:1.0 averager_counter {
    FAST_COUNT_WIDTH $fast_count_width
    SLOW_COUNT_WIDTH $slow_count_width
  } {
    clk clk
    clken $clken
    count_max period
    n_avg n_avg
    avg_on avg_on
    wen shift_reg/SCLR
    address addr
    clr_fback sr_avg_off/SCLR
    avg_on_out avg_on_out
    srst [get_not_pin tvalid]
  }

  connect_pins wen [get_concat_pin [lrepeat 4 averager_counter/wen]]

  # Delay restart until n_avg >= n_avg_max
  cell koheron:user:delay_trig:1.0 delay_trig {} {
    clk clk
    trig_in restart
    valid [get_GE_pin averager_counter/slow_count n_avg_min]
    trig_out averager_counter/restart
  }

  connect_pins ready [get_and_pin delay_trig/ready averager_counter/ready]

  connect_pins new_cycle [get_edge_detector averager_counter/wen]

  current_bd_instance $bd
}

} ;# end namespace averager
