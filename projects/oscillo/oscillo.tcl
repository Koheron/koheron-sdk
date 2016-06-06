source projects/averager_module/averager.tcl

###########################################################
# Add ADC BRAMs
###########################################################

for {set i 0} {$i < 2} {incr i} {
  set channel [lindex {a b} $i]
  set adc_bram_name adc${i}_bram
  set avg_name      avg$i
  add_bram $adc_bram_name [set config::axi_adc[expr {$i + 1}]_range] [set config::axi_adc[expr {$i + 1}]_offset]
  # Connect port B of BRAM to ADC clock
  connect_constant ${adc_bram_name}_enb 1 1 blk_mem_gen_$adc_bram_name/enb
  connect_pins blk_mem_gen_$adc_bram_name/clkb    $adc_clk
  connect_pins blk_mem_gen_$adc_bram_name/rstb    $rst_adc_clk_name/peripheral_reset

  # Add averaging module
  add_averager_module $avg_name $config::bram_addr_width -input_type fix_$config::adc_width

  connect_pins $avg_name/clk         $adc_clk
  connect_pins $avg_name/restart     $address_name/restart

  connect_pins $avg_name/avg_on      [cfg_pin avg$i]
  connect_pins $avg_name/period      [cfg_pin period$i]
  connect_pins $avg_name/threshold   [cfg_pin threshold$i]
  connect_pins $avg_name/n_avg_min   [cfg_pin n_avg_min$i]
  connect_pins $avg_name/tvalid      $address_name/tvalid
  connect_pins $avg_name/din         adc_dac/adc[expr $i + 1]
  connect_pins $avg_name/addr        blk_mem_gen_$adc_bram_name/addrb
  connect_pins $avg_name/dout        blk_mem_gen_$adc_bram_name/dinb
  connect_pins $avg_name/wen         blk_mem_gen_$adc_bram_name/web
  connect_pins $avg_name/n_avg       [sts_pin n_avg$i]
  connect_pins $avg_name/ready       [sts_pin avg_ready$i]
  connect_pins $avg_name/avg_on_out  [sts_pin avg_on_out$i]
}
