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
add_config_register $config_name $adc_clk 16

source lib/status_register.tcl
set status_name sts
add_status_register $status_name $adc_clk 16
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

# Add GPIO
source boards/$board_name/gpio.tcl
add_gpio

# Add PWM
source boards/$board_name/pwm.tcl
add_pwm pwm $pwm_clk $pwm0_offset $pwm_width $n_pwm

for {set i 0} {$i < $n_pwm} {incr i} {
  set offset pwm${i}_offset
  connect_pins pwm/pwm$i  $config_name/Out[expr $$offset]
}

connect_pins pwm/rst $rst_name/peripheral_reset

# Add DAC BRAM
source lib/bram.tcl
set bram_size [expr 2**($bram_addr_width-8)]K
set dac_bram_name dac_bram 
add_bram $dac_bram_name $bram_size

# Add address module
source lib/address.tcl
set address_name   address
add_address_module $address_name $bram_addr_width $adc_clk
connect_pins $address_name/clk  $adc_clk
connect_pins $address_name/cfg  $config_name/Out$addr_offset
source lib/connect_dac_bram.tcl

