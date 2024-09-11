source $board_path/starting_point.tcl

# -----------------------------------------------------------------------------
# ADC
# -----------------------------------------------------------------------------

connect_pins [get_slice_pin [ctl_pin rf_adc_ctl0] 3 3] adc_dac/adc_clkout_dec
connect_pins [get_slice_pin [ctl_pin adp5071_sync] 0 0] adc_dac/adp5071_sync_en
connect_pins [get_slice_pin [ctl_pin adp5071_sync] 1 1] adc_dac/adp5071_sync_state

for {set i 0} {$i < 2} {incr i} {
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 0 0] adc${i}_ctl_range_sel
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 1 1] adc${i}_ctl_testpat
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 2 2] adc${i}_ctl_en

  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 8 4] adc_dac/adc${i}_dco_delay_tap
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 14 9] adc_dac/adc${i}_da_delay_tap
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 20 15] adc_dac/adc${i}_db_delay_tap
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 21 21] adc_dac/adc${i}_delay_rst
}

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
