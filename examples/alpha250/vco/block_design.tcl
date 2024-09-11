source ${board_path}/starting_point.tcl
# Removing extra delays because latency is critical
set_property -dict [list CONFIG.Depth {1}] [get_bd_cells adc_dac/Q_d2_noce_concat_adc0_dout]
set_property -dict [list CONFIG.Depth {1}] [get_bd_cells adc_dac/Q_d2_noce_concat_adc1_dout]
set_property -dict [list CONFIG.Depth {1}] [get_bd_cells adc_dac/Q_d2_noce_dac0]
set_property -dict [list CONFIG.Depth {1}] [get_bd_cells adc_dac/Q_d2_noce_dac1]


set phase_width 48
set adc_width [get_parameter adc_width]

for {set i 0} {$i < 2} {incr i} {

  cell koheron:user:right_shifter:1.0 right_shifter$i {
    TOTAL_WIDTH $phase_width
    DATA_WIDTH $adc_width
  } {
    clk adc_dac/adc_clk
    in adc_dac/adc$i
    shift [ctl_pin vco_gain$i]
  }

  cell xilinx.com:ip:c_addsub:12.0 adder$i {
    A_Width $phase_width
    A_Type Unsigned
    B_Width $phase_width
    B_Type Signed
    Out_Width $phase_width
    Latency 1
    CE false
  } {
    CLK adc_dac/adc_clk
    A [get_concat_pin [list [ctl_pin phase_incr[expr 2*$i]] [get_slice_pin [ctl_pin phase_incr[expr 2*$i + 1]] 15 0]]]
    B right_shifter$i/out
  }

  cell xilinx.com:ip:dds_compiler:6.0 dds$i {
    PartsPresent Phase_Generator_and_SIN_COS_LUT
    Output_Selection Sine
    DDS_Clock_Rate [expr [get_parameter adc_clk]/1000000]
    Parameter_Entry Hardware_Parameters
    Phase_Width $phase_width
    Output_Width 16
    Phase_Increment Streaming
    Has_Phase_Out false
    Latency_Configuration Configurable
    Latency 3
  } {
    aclk adc_dac/adc_clk
    s_axis_phase_tdata adder$i/S
    s_axis_phase_tvalid [get_constant_pin 1 1]
  }

  connect_pins adc_dac/dac[expr $i] dds$i/m_axis_data_tdata

}
