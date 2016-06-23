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

set n_periods 2

address::create $address_name $config::bram_addr_width $n_periods

connect_cell $address_name {
  clk  $adc_clk
  cfg  [cfg_pin addr]
}

# Add address interconnect

source $lib/interconnect.tcl
set addr_intercon_name addr_intercon
add_interconnect $addr_intercon_name [expr $config::bram_addr_width + 2] $n_periods $config::n_dac_bram

connect_cell $addr_intercon_name {
  clk $adc_clk
  sel [cfg_pin addr_select]
  clken [get_constant_pin 1 1]
}

for {set i 0} {$i < $n_periods} {incr i} {
  connect_pins $address_name/period$i  [cfg_pin dac_period$i]
  connect_pins $address_name/addr$i $addr_intercon_name/in$i
}

# Add DAC controller

source $lib/dac_controller.tcl
set bram_size [expr 2**($config::bram_addr_width-8)]K

set interconnect_name dac_interconnect
add_interconnect $interconnect_name $config::dac_width $config::n_dac_bram 2

connect_cell $interconnect_name {
  clk $adc_clk
  sel [cfg_pin dac_select]
  clken [get_constant_pin 1 1]
}

for {set i 1} {$i <= $config::n_dac_bram} {incr i} {
  set dac_controller_name dac${i}_ctrl 
  add_single_dac_controller $dac_controller_name dac$i $config::dac_width
  connect_cell $dac_controller_name {
    clk  $adc_clk
    addr $addr_intercon_name/out[expr $i-1]
    rst  $rst_adc_clk_name/peripheral_reset
    dac  $interconnect_name/in[expr $i-1]
  }
} 

for {set i 1} {$i <= 2} {incr i} {
  connect_pins $interconnect_name/out[expr $i-1] $adc_dac_name/dac$i
}
