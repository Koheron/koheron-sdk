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

# Add DAC BRAM
source $lib/bram.tcl
set bram_size [expr 2**($config::bram_addr_width-8)]K
set dac_bram_name dac_bram 
add_bram $dac_bram_name $config::axi_dac_range $config::axi_dac_offset 00

source $lib/connect_dac_bram.tcl
