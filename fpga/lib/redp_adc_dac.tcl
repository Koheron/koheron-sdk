proc add_redp_adc_dac {module_name} {
  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  for {set i 1} {$i <= 2} {incr i} {
    create_bd_pin -dir I -from 13 -to 0 dac$i
    create_bd_pin -dir O -from 13 -to 0 adc$i
  }

  create_bd_pin -dir O adc_clk
  create_bd_pin -dir O ser_clk
  create_bd_pin -dir O pwm_clk

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
    CLKOUT6_USED true CLKOUT6_REQUESTED_OUT_FREQ 333.33
    USE_RESET false
  } {
    clk_out1 adc_clk
    clk_out5 ser_clk
    clk_out6 pwm_clk
  }
  
  foreach {pol} {p n} {connect_port_pin /adc_clk_${pol}_i pll/clk_in1_${pol}}

  # Add ADC IP block
  cell pavel-demin:user:redp_adc:1.0 adc {} {
    adc_clk     pll/clk_out1
    adc_dat_a_o adc1
    adc_dat_b_o adc2
  }
  
  connect_ports adc

  # Add DAC IP block
  cell pavel-demin:user:redp_dac:1.0 dac {} {
    dac_dat_a_i dac1
    dac_dat_b_i dac2
    dac_clk_1x pll/clk_out2
    dac_clk_2x pll/clk_out3
    dac_clk_2p pll/clk_out4
    dac_locked pll/locked
  }
  
  connect_ports dac

  # Connect reset
  cell xilinx.com:ip:xlconstant:1.1 adc_rst {} {
    dout adc/adc_rst_i
  }

  current_bd_instance $bd
}
