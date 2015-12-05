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

connect_pins $spectrum_name/tvalid     $address_name/tvalid
connect_pins $spectrum_name/cfg_sub    $config_name/Out[expr $spectrum_offset]
connect_pins $spectrum_name/cfg_fft    $config_name/Out[expr $spectrum_offset + 1]
connect_pins $spectrum_name/demod_data $config_name/Out[expr $spectrum_offset + 2]

# Add BRAM
add_bram $spectrum_bram_name $bram_size
# Connect port B of BRAM to ADC clock
connect_constant ${spectrum_bram_name}_enb 1 1 blk_mem_gen_$spectrum_bram_name/enb
connect_pins blk_mem_gen_$spectrum_bram_name/clkb    $adc_clk
connect_pins blk_mem_gen_$spectrum_bram_name/rstb    $rst_name/peripheral_reset

# Add averaging module
source projects/averager_float.tcl
set avg_name avg
add_averager_float_module $avg_name $bram_addr_width

connect_pins $avg_name/clk         $adc_clk
connect_pins $avg_name/restart     $address_name/restart
connect_pins $avg_name/avg_off     $config_name/Out[expr $spectrum_offset + 3]

connect_pins $spectrum_name/MAXIS_RESULT_tdata  $avg_name/din
connect_pins $spectrum_name/MAXIS_RESULT_tvalid $avg_name/tvalid

connect_pins $avg_name/addr        blk_mem_gen_$spectrum_bram_name/addrb
connect_pins $avg_name/dout        blk_mem_gen_$spectrum_bram_name/dinb
connect_pins $avg_name/wen         blk_mem_gen_$spectrum_bram_name/web

connect_pins $avg_name/n_avg       sts/In0
