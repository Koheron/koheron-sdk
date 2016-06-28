set clken [get_and_pin avg0/new_cycle [get_slice_pin [cfg_pin clken_mask] 0 0]]
connect_pins $addr_intercon_name/clken $clken
connect_pins $interconnect_name/clken $clken

# Counter
cell xilinx.com:ip:c_counter_binary:12.0 counter {Output_Width 64} {CLK $adc_clk}
set counter  [get_Q_pin counter/Q 1 $clken $adc_clk]

connect_pins [sts_pin counter0] [get_slice_pin $counter 31 0]
connect_pins [sts_pin counter1] [get_slice_pin $counter 63 32]
