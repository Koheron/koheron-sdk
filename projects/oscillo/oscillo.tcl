source projects/averager_module/averager.tcl
source $lib/adc_controller.tcl

###########################################################
# Add ADC BRAMs
###########################################################

for {set i 0} {$i < 2} {incr i} {
  set channel [lindex {a b} $i]
  set adc_bram_name adc${i}_bram
  set avg_name      avg$i

  set adc_controller_name adc_ctrl$i 
  add_adc_controller $adc_controller_name adc[expr {$i + 1}]

  connect_pins $adc_controller_name/clk   $adc_clk
  connect_pins $adc_controller_name/rst   $rst_adc_clk_name/peripheral_reset

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
  connect_pins $avg_name/addr        $adc_controller_name/addr
  connect_pins $avg_name/dout        $adc_controller_name/adc
  connect_pins $avg_name/wen         $adc_controller_name/wen
  connect_pins $avg_name/n_avg       [sts_pin n_avg$i]
  connect_pins $avg_name/ready       [sts_pin avg_ready$i]
  connect_pins $avg_name/avg_on_out  [sts_pin avg_on_out$i]
}
