source boards/$board_name/base_system.tcl

# Connect DAC to config and ADC to status
for {set i 0} {$i < [get_parameter n_adc]} {incr i} {
  connect_pins [cfg_pin dac$i] adc_dac/dac[expr $i+1]
  connect_pins [sts_pin adc$i] adc_dac/adc[expr $i+1]
}
