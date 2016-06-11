source boards/$board_name/base_system.tcl

# Add GPIO
source boards/$board_name/gpio.tcl
add_gpio

# Add PWM
source $lib/pwm.tcl
add_pwm pwm $pwm_clk $config::pwm0_offset $config::pwm_width $config::n_pwm

for {set i 0} {$i < $config::n_pwm} {incr i} {
  connect_pins pwm/pwm$i  [cfg_pin pwm${i}]
}

connect_pins pwm/rst $rst_adc_clk_name/peripheral_reset

# Add address module
source projects/address_module/address.tcl
set address_name address
add_address_module $address_name $config::bram_addr_width

connect_cell $address_name {
  clk  $adc_clk
  cfg  [cfg_pin addr]
  period  [cfg_pin period0]
}

# Add DAC controller

source $lib/dac_controller.tcl
set bram_size [expr 2**($config::bram_addr_width-8)]K
set dac_controller_name dac_ctrl 
add_dual_dac_controller $dac_controller_name dac $config::dac_width

connect_cell $dac_controller_name {
  clk  $adc_clk
  addr $address_name/addr
  rst  $rst_adc_clk_name/peripheral_reset
  dac0 $adc_dac_name/dac1
  dac1 $adc_dac_name/dac2
}
