namespace eval bram_accumulator {

proc pins {cmd} {
   $cmd -dir I -from 31 -to 0 s_axis_tdata
   $cmd -dir I -from 31 -to 0 addr_in
   $cmd -dir I                s_axis_tvalid
   $cmd -dir O -from 31 -to 0 m_axis_tdata
   $cmd -dir O -from 31 -to 0 addr_out
   $cmd -dir I -type clk      clk
   $cmd -dir I                first_cycle
   $cmd -dir I                last_cycle
   $cmd -dir O -from 3  -to 0 wen
}

proc create {module_name} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  pins create_bd_pin

  set add_latency 8
  set bram_latency 2
  set sr_latency 1
  set read_latency [expr $bram_latency + $sr_latency]
  set total_latency [expr $read_latency + $add_latency]

  set ce_pin [get_Q_pin s_axis_tvalid $read_latency]
  set wea_pin [get_Q_pin $ce_pin $add_latency]
  set addr_pin [get_Q_pin addr_in $total_latency]

  # Add BRAM for accumulator (write on port A and read on port B)

  cell xilinx.com:ip:blk_mem_gen:8.3 accum_bram {
    Memory_Type True_Dual_Port_RAM
    use_bram_block Stand_Alone
    Enable_32bit_Address true
    Assume_Synchronous_Clk true
  } {
    clka clk
    clkb clk
    rsta [get_constant_pin 0 1]
    rstb [get_constant_pin 0 1]
    ena [get_constant_pin 1 1]
    enb [get_constant_pin 1 1]
    dinb [get_constant_pin 0 32]
    addra $addr_pin
    addrb addr_in
    wea [get_concat_pin [lrepeat 4 $wea_pin]]
    web [get_constant_pin 0 4]
  }

  cell xilinx.com:ip:c_shift_ram:12.0 shift_reg {
    SCLR true
    Width 32
    Depth $sr_latency
  } {
    CLK clk
    D accum_bram/doutb
    SCLR [get_Q_pin first_cycle $bram_latency]
  }

  cell xilinx.com:ip:floating_point:7.1 adder {
    A_Precision_Type.VALUE_SRC PROPAGATED
    Add_Sub_Value Add
    C_Optimization Low_Latency
    Flow_Control NonBlocking
    Maximum_Latency false
    C_Latency $add_latency
    C_Mult_Usage No_Usage
    C_Rate 1
    Has_ACLKEN true
  } {
    aclk clk
    aclken $ce_pin
    s_axis_a_tdata shift_reg/Q
    s_axis_b_tdata [get_Q_pin s_axis_tdata $read_latency]
    m_axis_result_tdata accum_bram/dina
  }

  set wen_pin [get_Q_pin last_cycle $total_latency]

  cell xilinx.com:ip:xlconcat:2.1 concat_wen {
    NUM_PORTS 4
  } {
    In0 $wen_pin
    In1 $wen_pin
    In2 $wen_pin
    In3 $wen_pin
    dout wen
  }

  connect_pins m_axis_tdata adder/m_axis_result_tdata
  connect_pins addr_out $addr_pin

  current_bd_instance $bd
}

} ;# end namespace bram_accumulator
