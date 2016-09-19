source boards/$board_name/config/ports.tcl
source boards/$board_name/base_system.tcl

# Add GPIO
source boards/$board_name/gpio.tcl
add_gpio

# Add pulse density modulator for Red Pitaya slow DACs
set n_pwm [get_parameter n_pwm]
cell xilinx.com:ip:xlconcat:2.1 concat_pwm {NUM_PORTS $n_pwm} {}
connect_bd_net [get_bd_ports dac_pwm_o] [get_bd_pins concat_pwm/dout]

for {set i 0} {$i < $n_pwm} {incr i} {
  cell koheron:user:pdm:1.0 pwm_$i {
    NBITS [get_parameter pwm_width]
  } {
    clk adc_dac/pwm_clk
    rst $rst_adc_clk_name/peripheral_reset
    data_in [cfg_pin pwm$i]
    pdm_out concat_pwm/In$i
  }
}

# Add address module
source fpga/modules/address/address.tcl
set address_name address

address::create $address_name [expr [get_memory_addr_width dac0] + 1] [get_parameter n_dac]

connect_cell $address_name {
  clk  $adc_clk
  cfg  [cfg_pin addr]
}

# Add Dac router
source fpga/lib/dac_router.tcl
add_dac_router dac_router dac0 1

for {set i 0} {$i <  [get_parameter n_dac]} {incr i} {
  connect_pins $address_name/period$i  [cfg_pin dac_period$i]
  connect_pins $address_name/addr$i  dac_router/addr$i
}

connect_cell dac_router {
  clk $adc_clk
  rst $rst_adc_clk_name/peripheral_reset
  addr_select [cfg_pin addr_select]
  dac_select [cfg_pin dac_select]
}

for {set i 0} {$i < [get_parameter n_dac]} {incr i} {
  connect_pins dac_router/dout$i $adc_dac_name/dac[expr $i+1]
}

