source fpga/modules/averager/averager.tcl
source $lib/bram_recorder.tcl

###########################################################
# Add ADC BRAM recorders
###########################################################

for {set i 0} {$i < 2} {incr i} {
  set channel [lindex {a b} $i]
  set adc_bram_name adc${i}_bram
  set avg_name      avg$i

  set adc_recorder_name adc_recorder$i
  add_bram_recorder $adc_recorder_name adc$i  1

  connect_cell $adc_recorder_name {
    clk   $adc_clk
    rst   $rst_adc_clk_name/peripheral_reset
  }

  # Add averaging module
  averager::create $avg_name [get_memory_addr_width adc0] -input_type fix_[get_parameter adc_width]

  connect_cell $avg_name {
    clk         $adc_clk
    restart     $address_name/restart
    avg_on      [cfg_pin avg$i]
    period      [cfg_pin avg_period]
    threshold   [cfg_pin avg_threshold]
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
