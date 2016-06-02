proc add_redp_adc_dac {module_name} {
  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I -from 13 -to 0 dac1
  create_bd_pin -dir I -from 13 -to 0 dac2

  create_bd_pin -dir O adc_clk
  create_bd_pin -dir O ser_clk
  create_bd_pin -dir O pwm_clk

  create_bd_pin -dir O -from 13 -to 0 adc1
  create_bd_pin -dir O -from 13 -to 0 adc2

  # Phase-locked Loop (PLL)
  cell xilinx.com:ip:clk_wiz:5.3 pll {
    PRIMITIVE              PLL
    PRIM_IN_FREQ.VALUE_SRC USER
    PRIM_IN_FREQ           125.0
    PRIM_SOURCE            Differential_clock_capable_pin
    CLKOUT1_USED true CLKOUT1_REQUESTED_OUT_FREQ 125.0
    CLKOUT2_USED true CLKOUT2_REQUESTED_OUT_FREQ 125.0
    CLKOUT3_USED true CLKOUT3_REQUESTED_OUT_FREQ 250.0
    CLKOUT4_USED true CLKOUT4_REQUESTED_OUT_FREQ 250.0 CLKOUT4_REQUESTED_PHASE -45
    CLKOUT5_USED true CLKOUT5_REQUESTED_OUT_FREQ 250.0
    CLKOUT6_USED true CLKOUT6_REQUESTED_OUT_FREQ 250.0
    USE_RESET false
  } {}
  connect_bd_net [get_bd_ports /adc_clk_p_i] [get_bd_pins pll/clk_in1_p]
  connect_bd_net [get_bd_ports /adc_clk_n_i] [get_bd_pins pll/clk_in1_n]

  # Add ADC IP block
  create_bd_cell -type ip -vlnv pavel-demin:user:redp_adc:1.0 adc
  foreach {port_name} {
    adc_dat_a_i
    adc_dat_b_i
    adc_clk_source
    adc_cdcs_o
  } {
    connect_bd_net [get_bd_ports /$port_name] [get_bd_pins adc/$port_name]
  }
  connect_pins adc/adc_clk     pll/clk_out1
  connect_pins adc/adc_dat_a_o adc1
  connect_pins adc/adc_dat_b_o adc2

  # Add DAC IP block
  cell pavel-demin:user:redp_dac:1.0 dac {} {
    dac_dat_a_i dac1
    dac_dat_b_i dac2
  }
  foreach {port_name} {
    dac_clk_o
    dac_dat_o
    dac_rst_o
    dac_sel_o
    dac_wrt_o
  } {
    connect_bd_net [get_bd_ports /$port_name] [get_bd_pins dac/$port_name]
  }
  connect_pins dac/dac_clk_1x pll/clk_out2
  connect_pins dac/dac_clk_2x pll/clk_out3
  connect_pins dac/dac_clk_2p pll/clk_out4
  connect_pins dac/dac_locked pll/locked

  # Connect reset
  create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 adc_rst

  connect_pins adc_rst/dout adc/adc_rst_i

  connect_pins adc_clk pll/clk_out1
  connect_pins ser_clk pll/clk_out5
  connect_pins pwm_clk pll/clk_out6

  current_bd_instance $bd

}
