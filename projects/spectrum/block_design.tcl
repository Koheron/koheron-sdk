##########################################################
# Define offsets
##########################################################
set led_offset      0
set pwm_offset      1
set addr_offset     5
set spectrum_offset 6

##########################################################
# Define parameters
##########################################################
set bram_addr_width 12
set pwm_width       10
set n_pwm           4

set bram_size [expr 2**($bram_addr_width-8)]K

source boards/$board_name/base_system.tcl

source projects/spectrum.tcl

set spectrum_name spectrum_0


set spectrum_bram_name spectrum_bram
set n_pts_fft [expr 2**$bram_addr_width]

# Add spectrum IP
add_spectrum $spectrum_name $n_pts_fft $adc_width $adc_clk

for {set i 1} {$i < 3} {incr i} {
  connect_pins spectrum_0/adc$i adc_dac/adc$i
}

connect_pins $spectrum_name/tvalid  $address_name/tvalid
connect_pins $spectrum_name/cfg_sub $config_name/Out[expr $spectrum_offset]
connect_pins $spectrum_name/cfg_fft $config_name/Out[expr $spectrum_offset + 1]

# Add BRAM
add_bram $spectrum_bram_name $bram_size
