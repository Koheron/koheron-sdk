set module_name adc_dac
set bd [current_bd_instance .]
current_bd_instance [create_bd_cell -type hier $module_name]

# Phase-locked Loop (PLL)
cell xilinx.com:ip:clk_wiz:5.2 pll {
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
set adc_name adc_0
create_bd_cell -type ip -vlnv pavel-demin:user:redp_adc:1.0 $adc_name
foreach {port_name} {
  adc_dat_a_i
  adc_dat_b_i
  adc_clk_source
  adc_cdcs_o
} {
  connect_bd_net [get_bd_ports /$port_name] [get_bd_pins $adc_name/$port_name]
}
connect_bd_net [get_bd_pins $adc_name/adc_clk] [get_bd_pins pll/clk_out1]

# Add DAC IP block
set dac_name dac_0
create_bd_cell -type ip -vlnv pavel-demin:user:redp_dac:1.0 $dac_name
foreach {port_name} {
  dac_clk_o
  dac_dat_o
  dac_rst_o
  dac_sel_o
  dac_wrt_o
} {
  connect_bd_net [get_bd_ports /$port_name] [get_bd_pins $dac_name/$port_name]
}
connect_bd_net [get_bd_pins $dac_name/dac_clk_1x] [get_bd_pins pll/clk_out2]
connect_bd_net [get_bd_pins $dac_name/dac_clk_2x] [get_bd_pins pll/clk_out3]
connect_bd_net [get_bd_pins $dac_name/dac_clk_2p] [get_bd_pins pll/clk_out4]
connect_bd_net [get_bd_pins $dac_name/dac_locked] [get_bd_pins pll/locked]

# Connect reset
create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 adc_rst

connect_bd_net [get_bd_pins adc_rst/dout] [get_bd_pins $adc_name/adc_rst_i]

create_bd_pin -dir O adc_clk
connect_bd_net [get_bd_pins adc_clk] [get_bd_pins pll/clk_out1]

create_bd_pin -dir O pwm_clk
connect_bd_net [get_bd_pins pwm_clk] [get_bd_pins pll/clk_out6]

current_bd_instance $bd
