
# Add PS and AXI Interconnect
set board_preset $board_path/config/board_preset.tcl
source $lib_path/starting_point.tcl

# Add ADCs and DACs
source $lib_path/redp_adc_dac.tcl
set adc_dac_name adc_dac
add_redp_adc_dac $adc_dac_name

# Add processor system reset synchronous to adc clock
set rst_adc_clk_name proc_sys_reset_adc_clk

# Rename clocks
set adc_clk $adc_dac_name/adc_clk

cell xilinx.com:ip:proc_sys_reset:5.0 $rst_adc_clk_name {} {
  ext_reset_in $ps_name/FCLK_RESET0_N
  slowest_sync_clk $adc_clk
}

# Add control and status registers
source $lib_path/ctl_sts.tcl
add_ctl_sts $adc_clk $rst_adc_clk_name/peripheral_aresetn

# Connect LEDs
connect_port_pin led_o [get_slice_pin [ctl_pin led] 7 0]

# Add XADC
source $lib_path/xadc.tcl
set xadc_name xadc_wiz_0
add_xadc $xadc_name
