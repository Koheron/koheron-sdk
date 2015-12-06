source scripts/bram.tcl
source projects/init_bd.tcl
source boards/$board_name/gpio.tcl
source projects/config_register.tcl
source projects/status_register.tcl
source boards/$board_name/pwm.tcl
source projects/address.tcl

set board_preset boards/$board_name/config/board_preset.xml

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
# Define parameters
##########################################################
set dac_width       14
set adc_width       14

##########################################################
# Init block design and add XADC
##########################################################
init_bd $board_preset $xadc_name

##########################################################
# Add GPIO
##########################################################
add_gpio

##########################################################
# Add ADCs and DACs
##########################################################
source boards/$board_name/adc_dac.tcl
# Rename clocks
set adc_clk adc_dac/adc_clk
set pwm_clk adc_dac/pwm_clk

# Add Configuration register (synchronous with ADC clock)
##########################################################
add_config_register $config_name $adc_clk 16

##########################################################
# Add Status register
##########################################################
add_status_register $status_name $adc_clk 4

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
# Add PWM
##########################################################
add_pwm pwm $pwm_clk $pwm_offset $pwm_width 4
connect_pins pwm/cfg  $config_name/cfg
for {set i 0} {$i < $n_pwm} {incr i} {
  connect_pins pwm/pwm$i  $config_name/Out[expr $pwm_offset + $i]
}

##########################################################
# Add address module
##########################################################
add_address_module $address_name $bram_addr_width $adc_clk
connect_pins $address_name/clk  $adc_clk
connect_pins $address_name/cfg  $config_name/Out$addr_offset

##########################################################
# Add DAC BRAM
##########################################################
add_bram $dac_bram_name $bram_size
# Connect port B of BRAM to ADC clock
connect_pins blk_mem_gen_$dac_bram_name/clkb    $adc_clk
connect_pins blk_mem_gen_$dac_bram_name/addrb   $address_name/addr_delayed
# Connect BRAM output to DACs
for {set i 0} {$i < 2} {incr i} {
  set channel [lindex {a b} $i]
  cell xilinx.com:ip:xlslice:1.0 dac_${channel}_slice {
    DIN_WIDTH 32
    DIN_FROM [expr $dac_width-1+16*$i]
    DIN_TO [expr 16*$i]
  } {
    Din blk_mem_gen_$dac_bram_name/doutb
    Dout adc_dac/dac/dac_dat_${channel}_i
  }
}
# Connect remaining ports of BRAM
connect_constant ${dac_bram_name}_dinb 0 32 blk_mem_gen_$dac_bram_name/dinb
connect_constant ${dac_bram_name}_enb  1 1  blk_mem_gen_$dac_bram_name/enb
connect_constant ${dac_bram_name}_web  0 4  blk_mem_gen_$dac_bram_name/web
connect_pins blk_mem_gen_$dac_bram_name/rstb     $rst_name/peripheral_reset


