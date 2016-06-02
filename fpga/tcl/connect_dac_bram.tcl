# Connect port B of BRAM to ADC clock
connect_pins blk_mem_gen_$dac_bram_name/clkb    $adc_clk
connect_pins blk_mem_gen_$dac_bram_name/addrb   $address_name/addr
# Connect BRAM output to DACs
for {set i 0} {$i < 2} {incr i} {
  cell xilinx.com:ip:xlslice:1.0 dac[expr $i+1]_slice {
    DIN_WIDTH 32
    DIN_FROM [expr $config::dac_width-1+16*$i]
    DIN_TO [expr 16*$i]
  } {
    Din blk_mem_gen_$dac_bram_name/doutb
    Dout $adc_dac_name/dac[expr $i + 1]
  }
}
# Connect remaining ports of BRAM
connect_constant ${dac_bram_name}_dinb 0 32 blk_mem_gen_$dac_bram_name/dinb
connect_constant ${dac_bram_name}_enb  1 1  blk_mem_gen_$dac_bram_name/enb
connect_constant ${dac_bram_name}_web  0 4  blk_mem_gen_$dac_bram_name/web
connect_pins blk_mem_gen_$dac_bram_name/rstb  $rst_adc_clk_name/peripheral_reset
