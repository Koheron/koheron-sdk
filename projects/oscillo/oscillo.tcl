source lib/averager.tcl

###########################################################
# Add ADC BRAMs
###########################################################
for {set i 0} {$i < 2} {incr i} {
  set channel [lindex {a b} $i]
  set adc_bram_name adc${i}_bram
  set avg_name      avg$i
  add_bram $adc_bram_name [set axi_adc[expr {$i + 1}]_range] [set axi_adc[expr {$i + 1}]_offset]
  # Connect port B of BRAM to ADC clock
  connect_constant ${adc_bram_name}_enb 1 1 blk_mem_gen_$adc_bram_name/enb
  connect_pins blk_mem_gen_$adc_bram_name/clkb    $adc_clk
  connect_pins blk_mem_gen_$adc_bram_name/rstb    $rst_adc_clk_name/peripheral_reset

  # Add averaging module
  add_averager_module $avg_name $bram_addr_width -input_type fix_$adc_width

  connect_pins $avg_name/clk         $adc_clk
  connect_pins $avg_name/restart     $address_name/restart
  connect_pins $avg_name/avg_off     $config_name/Out[set avg${i}_offset]
  connect_pins $avg_name/tvalid      $address_name/tvalid
  connect_pins $avg_name/din         adc_dac/adc[expr $i + 1]
  connect_pins $avg_name/addr        blk_mem_gen_$adc_bram_name/addrb
  connect_pins $avg_name/dout        blk_mem_gen_$adc_bram_name/dinb
  connect_pins $avg_name/wen         blk_mem_gen_$adc_bram_name/web
  connect_pins $avg_name/n_avg       sts/In[set n_avg${i}_offset]
}
