source boards/$board_name/base_system.tcl

# Connect DAC1 to config and ADC1 to status
connect_pins $status_name/In$config::adc1_offset adc_dac/adc1

# Add PID controller
source lib/pid_controller.tcl
set pid_out_width 32
add_pid pid_controller [expr $config::adc_width + 1] $pid_out_width
connect_pins pid_controller/clk $adc_clk

foreach {name} {p i d} {
  connect_pins pid_controller/coef_$name $config_name/Out[set config::coef_${name}_offset]
}
connect_pins pid_controller/integral_reset $config_name/Out$config::integral_reset_offset

# Connect output of PID controller to DAC2
cell xilinx.com:ip:xlslice:1.0 slice_pid_cmd_out {
  DIN_FROM [expr $pid_out_width - 1]
  DIN_TO [expr $pid_out_width - $config::dac_width]
} {
  Din pid_controller/cmd_out
  Dout adc_dac/dac2
}

# error_in = set_point - adc2
cell xilinx.com:ip:c_addsub:12.0 set_point_minus_signal {
  Add_Mode Subtract
  CE false
  Latency 1
  Out_Width [expr $config::adc_width + 1]
} {
  clk $adc_clk
  A $config_name/Out$config::set_point_offset
  B adc_dac/adc2
  S pid_controller/error_in
}

# Connect error_in and cmd_out to status
connect_pins set_point_minus_signal/B $status_name/In$config::error_in_offset
connect_pins pid_controller/cmd_out $status_name/In$config::cmd_out_offset


# Add DDS
set dds_name dds
cell xilinx.com:ip:dds_compiler:6.0 $dds_name {
 DDS_Clock_Rate 125
 Parameter_Entry Hardware_Parameters
 Phase_Width 32
 Output_Width 14
 Phase_Increment Streaming
 Has_Phase_Out false
 Latency_Configuration Configurable
 Latency 2
 Noise_Shaping None
 Output_Width 14
 DATA_Has_TLAST Not_Required
 S_PHASE_Has_TUSER Not_Required
 M_DATA_Has_TUSER Not_Required
 Output_Frequency1 0
} {
  aclk $adc_clk
}

# Control phase increment

cell pavel-demin:user:axis_constant:1.0 phase_inc {
  AXIS_TDATA_WIDTH 32
} {
  aclk $adc_clk
  M_AXIS $dds_name/S_AXIS_PHASE
  cfg_data $config_name/Out$config::dds_offset
}


cell xilinx.com:ip:xlslice:1.0 slice_dac {
  DIN_FROM 13
  DIN_TO 0
} {
  Din $dds_name/m_axis_data_tdata
  Dout adc_dac/dac1
}
