source boards/$board_name/base_system.tcl

source projects/spectrum.tcl

add_spectrum spectrum_0 $bram_addr_width $adc_width $adc_clk

for {set i 1} {$i < 3} {incr i} {
  connect_pins spectrum_0/adc$i adc_dac/adc$i
}
