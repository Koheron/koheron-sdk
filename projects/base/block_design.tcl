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
connect_pins $address_name/clk  $adc_clk
connect_pins $address_name/cfg  [cfg_pin addr]
connect_pins $address_name/period  [cfg_pin period0]

# Add DAC controller

source $lib/dac_controller.tcl
set bram_size [expr 2**($config::bram_addr_width-8)]K

for {set i 1} {$i <= 2} {incr i} {
  set dac_controller_name dac${i}_ctrl 
  add_single_dac_controller $dac_controller_name dac$i $config::dac_width

  connect_pins $dac_controller_name/clk  $adc_clk
  connect_pins $dac_controller_name/addr $address_name/addr
  connect_pins $dac_controller_name/rst  $rst_adc_clk_name/peripheral_reset
  connect_pins $dac_controller_name/dac  $adc_dac_name/dac$i
} 

