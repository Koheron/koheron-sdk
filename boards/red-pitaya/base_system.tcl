source projects/$project_name/config.tcl
source lib/utilities.tcl

# Add PS and AXI Interconnect
set board_preset boards/$board_name/config/board_preset.tcl
source lib/starting_point.tcl

# Add ADCs and DACs
source lib/redp_adc_dac.tcl
# Rename clocks
set adc_clk adc_dac/adc_clk
set pwm_clk adc_dac/pwm_clk

# Add config and status registers
source lib/config_register.tcl
set config_name cfg
add_config_register $config_name $adc_clk $config_size $axi_config_range $axi_config_offset 00

source lib/status_register.tcl
set status_name sts
add_status_register $status_name $adc_clk $status_size $axi_status_range $axi_status_offset
source lib/sha_dna.tcl

# Connect LEDs
cell xilinx.com:ip:xlslice:1.0 led_slice {
  DIN_WIDTH 32
  DIN_FROM  7
  DIN_TO    0
} {
  Din $config_name/Out[expr $led_offset]
}
connect_bd_net [get_bd_ports led_o] [get_bd_pins led_slice/Dout]

# Add XADC
source lib/xadc.tcl
set xadc_name xadc_wiz_0
add_xadc $xadc_name
