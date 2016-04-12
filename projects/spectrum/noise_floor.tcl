
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
  connect_pins blk_mem_gen_$bram_name/clkb  clk

  # Connect remaining ports of BRAM
  connect_constant ${bram_name}_dinb 0 32 blk_mem_gen_$bram_name/dinb
  connect_constant ${bram_name}_enb  1 1  blk_mem_gen_$bram_name/enb
  connect_constant ${bram_name}_web  0 4  blk_mem_gen_$bram_name/web
  connect_pins blk_mem_gen_$bram_name/rstb   /$::rst_adc_clk_name/peripheral_reset

  connect_pins blk_mem_gen_$bram_name/addrb address_counter/Q

  cell xilinx.com:ip:c_shift_ram:12.0 tdata_reg {
    Width 32
    Depth 1
  } {
    CLK clk
    D s_axis_tdata
  }

  cell xilinx.com:ip:c_shift_ram:12.0 tvalid_reg {
    Width 1
    Depth 1
  } {
    CLK clk
    D s_axis_tvalid
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
    s_axis_a_tvalid tvalid_reg/Q
    s_axis_b_tvalid tvalid_reg/Q
    s_axis_a_tdata tdata_reg/Q
    s_axis_b_tdata blk_mem_gen_$bram_name/doutb
    m_axis_result_tdata m_axis_result_tdata
    m_axis_result_tvalid m_axis_result_tvalid
}

current_bd_instance $bd

}
