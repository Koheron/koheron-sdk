source boards/$board_name/base_system.tcl

source projects/spectrum.tcl

set spectrum_name spectrum_0
set spectrum_offset 6

set spectrum_bram_name spectrum_bram
set n_pts_fft 4096

# Add spectrum IP
add_spectrum $spectrum_name $n_pts_fft $adc_width $adc_clk

for {set i 1} {$i < 3} {incr i} {
  connect_pins spectrum_0/adc$i adc_dac/adc$i
}

connect_pins $spectrum_name/cfg_sub $config_name/Out[expr $spectrum_offset]
connect_pins $spectrum_name/cfg_fft $config_name/Out[expr $spectrum_offset + 1]

# Add BRAM
set spectrum_bram_size [expr $n_pts_fft/256]K
add_bram $spectrum_bram_name $spectrum_bram_size
