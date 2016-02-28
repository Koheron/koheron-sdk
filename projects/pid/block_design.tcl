source boards/$board_name/base_system.tcl

# Connect DAC1 to config and ADC1 to status
connect_pins $config_name/Out[set dac1_offset] adc_dac/dac1
connect_pins $status_name/In[set adc1_offset] adc_dac/adc1

# Add PID controller
source lib/pid_controller.tcl
set pid_out_width 32
add_pid pid_controller [expr $adc_width + 1] $pid_out_width
connect_pins pid_controller/clk $adc_clk

foreach {name} {p i d} {
  connect_pins pid_controller/coef_$name $config_name/Out[set coef_${name}_offset]
}
connect_pins pid_controller/integral_reset $config_name/Out$integral_reset_offset

# Connect output of PID controller to DAC2
cell xilinx.com:ip:xlslice:1.0 slice_pid_cmd_out {
  DIN_FROM [expr $pid_out_width - 1]
  DIN_TO [expr $pid_out_width - $dac_width]
} {
  Din pid_controller/cmd_out
  Dout adc_dac/dac2
}

# error_in = set_point - adc2
cell xilinx.com:ip:c_addsub:12.0 set_point_minus_signal {
  Add_Mode Subtract
  CE false
  Latency 2
  Out_Width [expr $adc_width + 1]
} {
  clk $adc_clk
  A $config_name/Out$set_point_offset
  B adc_dac/adc2
  S pid_controller/error_in
}

# Connect error_in and cmd_out to status
connect_pins set_point_minus_signal/B $status_name/In$error_in_offset
connect_pins pid_controller/cmd_out $status_name/In$cmd_out_offset
