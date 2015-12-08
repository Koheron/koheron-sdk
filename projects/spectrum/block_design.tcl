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

# Add spectrum BRAM
set spectrum_bram_name spectrum_bram
add_bram $spectrum_bram_name $bram_size
connect_pins blk_mem_gen_$spectrum_bram_name/clkb $adc_clk
connect_pins blk_mem_gen_$spectrum_bram_name/rstb $rst_name/peripheral_reset
connect_pins blk_mem_gen_$spectrum_bram_name/enb ${dac_bram_name}_enb/dout

# Add demod BRAM
set demod_bram_name    demod_bram
add_bram $demod_bram_name $bram_size
connect_pins blk_mem_gen_$demod_bram_name/clkb  $adc_clk
connect_pins blk_mem_gen_$demod_bram_name/rstb  $rst_name/peripheral_reset
connect_pins blk_mem_gen_$demod_bram_name/web   ${dac_bram_name}_web/dout
connect_pins blk_mem_gen_$demod_bram_name/dinb  ${dac_bram_name}_dinb/dout
connect_pins blk_mem_gen_$demod_bram_name/enb   ${dac_bram_name}_enb/dout
connect_pins blk_mem_gen_$demod_bram_name/doutb $spectrum_name/demod_data
connect_pins blk_mem_gen_$demod_bram_name/addrb $address_name/addr

# Add averaging module
source projects/averager.tcl
set avg_name avg
add_averager_module $avg_name $bram_addr_width

connect_pins $avg_name/clk         $adc_clk
connect_pins $avg_name/restart     $address_name/restart
connect_pins $avg_name/avg_off     $config_name/Out[expr $spectrum_offset + 2]

connect_pins $spectrum_name/m_axis_result_tdata  $avg_name/din
connect_pins $spectrum_name/m_axis_result_tvalid $avg_name/tvalid

connect_pins $avg_name/addr        blk_mem_gen_$spectrum_bram_name/addrb
connect_pins $avg_name/dout        blk_mem_gen_$spectrum_bram_name/dinb
connect_pins $avg_name/wen         blk_mem_gen_$spectrum_bram_name/web

connect_pins $avg_name/n_avg       sts/In0
