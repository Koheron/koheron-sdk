source boards/$board_name/base_system.tcl

# Connect DAC to config and ADC to status
for {set i 1} {$i < 3} {incr i} {
  connect_pins $config_name/Out[set config::dac${i}_offset] adc_dac/dac$i
  connect_pins $status_name/In[set config::adc${i}_offset] adc_dac/adc$i
}
