source projects/averager_module/averager.tcl
source $lib/bram_recorder.tcl

###########################################################
# Add ADC BRAM recorders
###########################################################

for {set i 0} {$i < 2} {incr i} {
  set channel [lindex {a b} $i]
  set adc_bram_name adc${i}_bram
  set avg_name      avg$i

  set adc_recorder_name adc_recorder$i 
  add_bram_recorder $adc_recorder_name adc[expr {$i + 1}]

  connect_pins $adc_recorder_name/clk   $adc_clk
  connect_pins $adc_recorder_name/rst   $rst_adc_clk_name/peripheral_reset

  # Add averaging module
  add_averager_module $avg_name $config::bram_addr_width -input_type fix_$config::adc_width

  connect_cell $avg_name {
    clk         $adc_clk
    restart     $address_name/restart
    avg_on      [cfg_pin avg$i]
    period      [cfg_pin period$i]
    threshold   [cfg_pin threshold$i]
    n_avg_min   [cfg_pin n_avg_min$i]
    tvalid      $address_name/tvalid
    din         adc_dac/adc[expr $i + 1]
    addr        $adc_recorder_name/addr
    dout        $adc_recorder_name/adc
    wen         $adc_recorder_name/wen
    n_avg       [sts_pin n_avg$i]
    ready       [sts_pin avg_ready$i]
    avg_on_out  [sts_pin avg_on_out$i]
  }
}
