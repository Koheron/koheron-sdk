# Timer
source fpga/lib/timer.tcl
set timer_name timer
timer::create $timer_name

connect_cell $timer_name {
  clk $adc_clk
  cfg [cfg_pin clken_mask]
  clken_in avg0/new_cycle
  counter0 [sts_pin counter0]
  counter1 [sts_pin counter1]
}
set clken $timer_name/clken_out

connect_pins dac_router/clken $clken
