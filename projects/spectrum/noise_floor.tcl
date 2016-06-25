
proc add_noise_floor_module {module_name bram_addr_width clk} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I -type clk      clk
  create_bd_pin -dir I -from 31 -to 0 s_axis_tdata
  create_bd_pin -dir I                s_axis_tvalid

  #create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:axis_rtl:1.0 S_AXIS

  create_bd_pin -dir O -from 31 -to 0 m_axis_result_tdata
  create_bd_pin -dir O                m_axis_result_tvalid

  cell xilinx.com:ip:c_counter_binary:12.0 address_counter {
    CE true
    Output_Width [expr $bram_addr_width + 2]
    Increment_Value 4
  } {
    CLK clk
    CE s_axis_tvalid
  }

  set bram_name noise_floor_bram
  add_bram $bram_name $::config::axi_noise_floor_range $::config::axi_noise_floor_offset

  # TODO add rst port
  connect_cell blk_mem_gen_$bram_name {
    clkb clk
    addrb address_counter/Q
    rstb /$::rst_adc_clk_name/peripheral_reset
    dinb [get_constant_pin 0 32]
    enb  [get_constant_pin 1 1]
    web  [get_constant_pin 0 4]
  }

  set subtract_name subtract_noise_floor
  cell xilinx.com:ip:floating_point:7.1 $subtract_name {
    Add_Sub_Value Subtract
    C_Optimization Low_Latency
    C_Mult_Usage No_Usage
    Flow_Control NonBlocking
    Maximum_Latency false
    C_Latency 4
  } {
    aclk clk
    s_axis_a_tvalid [get_Q_pin s_axis_tvalid 1 1]
    s_axis_b_tvalid [get_Q_pin s_axis_tvalid 1 1]
    s_axis_a_tdata [get_Q_pin s_axis_tdata 32 1]
    s_axis_b_tdata blk_mem_gen_$bram_name/doutb
    m_axis_result_tdata m_axis_result_tdata
    m_axis_result_tvalid m_axis_result_tvalid
}

current_bd_instance $bd

}
