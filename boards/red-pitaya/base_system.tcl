
# Add PS and AXI Interconnect
set board_preset $board_path/config/board_preset.tcl
source $sdk_path/fpga/lib/starting_point.tcl

# Add ADCs and DACs
source $sdk_path/fpga/lib/redp_adc_dac.tcl
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
source $sdk_path/fpga/lib/ctl_sts.tcl
add_ctl_sts $adc_clk $rst_adc_clk_name/peripheral_aresetn

# Connect LEDs
connect_port_pin led_o [get_slice_pin [ctl_pin led] 7 0]

cell_inline xilinx.com:inline_hdl:ilconcat:1.0 concat_interrupts {
  NUM_PORTS 1
} {
  dout ps_0/IRQ_F2P
}

# Add XADC
source $sdk_path/fpga/lib/xadc.tcl
add_xadc xadc
