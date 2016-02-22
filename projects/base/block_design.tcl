source projects/base/xadc.tcl

source boards/$board_name/gpio.tcl
source boards/$board_name/pwm.tcl

source lib/utilities.tcl
source lib/init_bd.tcl
source lib/bram.tcl
source lib/config_register.tcl
source lib/status_register.tcl
source lib/address.tcl

set board_preset boards/$board_name/config/board_preset.tcl

##########################################################
# Define global variables
##########################################################
set ps_name        ps_1
set rst_name       rst_${ps_name}_125M

##########################################################
# Define block names
##########################################################
set xadc_name      xadc_wiz_0
set config_name    cfg
set status_name    sts
set address_name   address
set dac_bram_name  dac_bram
set adc1_bram_name adc1_bram
set avg_name       averaging

##########################################################
# Init block design and add DAC BRAM
##########################################################
init_bd $board_preset $dac_bram_name $bram_size

##########################################################
# Add GPIO
##########################################################
add_gpio

##########################################################
# Add ADCs and DACs
##########################################################
source lib/redp_adc_dac.tcl
# Rename clocks
set adc_clk adc_dac/adc_clk
set pwm_clk adc_dac/pwm_clk

# Add Configuration register (synchronous with ADC clock)
##########################################################
add_config_register $config_name $adc_clk 16

##########################################################
# Add Status register
##########################################################
add_status_register $status_name $adc_clk 16

for {set i 0} {$i < 8} {incr i} {
  set sha sha${i}
  connect_constant sha_constant_$i [expr $$sha] 32 $status_name/In$i
}
##########################################################
# Connect LEDs
##########################################################
cell xilinx.com:ip:xlslice:1.0 led_slice {
  DIN_WIDTH 32
  DIN_FROM  7
  DIN_TO    0
} {
  Din $config_name/Out[expr $led_offset]
}
connect_bd_net [get_bd_ports led_o] [get_bd_pins led_slice/Dout]

##########################################################
# Add XADC
##########################################################
add_xadc $xadc_name

##########################################################
# Add PWM
##########################################################
add_pwm pwm $pwm_clk $pwm0_offset $pwm_width $n_pwm

for {set i 0} {$i < $n_pwm} {incr i} {
  set offset pwm${i}_offset
  connect_pins pwm/pwm$i  $config_name/Out[expr $$offset]
}

connect_pins pwm/rst $rst_name/peripheral_reset

##########################################################
# Add address module
##########################################################
add_address_module $address_name $bram_addr_width $adc_clk
connect_pins $address_name/clk  $adc_clk
connect_pins $address_name/cfg  $config_name/Out$addr_offset

##########################################################
# DAC BRAM
##########################################################

source lib/connect_dac_bram.tcl


