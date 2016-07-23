
# Add PS and AXI Interconnect
set board_preset boards/$board_name/config/board_preset.tcl
source $lib/starting_point.tcl

# Add ADCs and DACs
source $lib/redp_adc_dac.tcl
set adc_dac_name adc_dac
add_redp_adc_dac $adc_dac_name

# Add processor system reset synchronous to adc clock
set rst_adc_clk_name proc_sys_reset_adc_clk

# Rename clocks
set adc_clk $adc_dac_name/adc_clk
set pwm_clk $adc_dac_name/pwm_clk

cell xilinx.com:ip:proc_sys_reset:5.0 $rst_adc_clk_name {} {
  ext_reset_in $ps_name/FCLK_RESET0_N
  slowest_sync_clk $adc_clk
}

# Add config and status registers
source $lib/config_register.tcl
set config_name cfg
add_config_register $config_name $adc_clk $config::config_size $config::axi_config_range $config::axi_config_offset

source $lib/status_register.tcl
set status_name sts
add_status_register $status_name $adc_clk $config::status_size $config::axi_status_range $config::axi_status_offset

# Connect LEDs
connect_bd_net [get_bd_ports led_o] [get_bd_pins [get_slice_pin [cfg_pin led] 7 0]]

# Add XADC
source $lib/xadc.tcl
set xadc_name xadc_wiz_0
add_xadc $xadc_name
