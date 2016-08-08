source boards/$board_name/base_system.tcl

# Connect DAC to config and ADC to status
for {set i 1} {$i < 3} {incr i} {
  connect_pins [cfg_pin dac$i] adc_dac/dac$i
  connect_pins [sts_pin adc$i] adc_dac/adc$i
}
