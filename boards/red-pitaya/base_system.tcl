source scripts/bram.tcl
source projects/init_bd.tcl
source boards/$board_name/gpio.tcl
source projects/config_register.tcl
source boards/$board_name/pwm.tcl
source projects/averaging.tcl
source projects/address.tcl

set board_preset boards/$board_name/config/board_preset.xml

##########################################################
# Define global variables
##########################################################
set ps_name        ps_1

##########################################################
# Define block names
##########################################################
set xadc_name      xadc_wiz_0
set config_name    cfg
set address_name   address
set dac_bram_name  dac_bram
set adc1_bram_name adc1_bram
set avg_name       averaging

##########################################################
# Define offsets
##########################################################
set led_offset   0
set pwm_offset   32
set addr_offset  [expr 5*32]
set avg_offset   [expr 6*32]

##########################################################
# Define parameters
##########################################################
set bram_addr_width 13
set dac_width       14
set adc_width       14
set pwm_width       10

set bram_size [expr 2**($bram_addr_width-8)]K

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
add_config_register $config_name $adc_clk

##########################################################
# Add Status register
##########################################################
# TODO

##########################################################
# Connect LEDs
##########################################################
cell xilinx.com:ip:xlslice:1.0 led_slice \
  [list                                  \
    DIN_WIDTH 1024                       \
    DIN_FROM  [expr 7+$led_offset]       \
    DIN_TO    [expr $led_offset]]        \
  [list Din $config_name/cfg]
connect_bd_net [get_bd_ports led_o] [get_bd_pins led_slice/Dout]

##########################################################
# Add PWM
##########################################################
add_pwm pwm $pwm_clk $pwm_offset $pwm_width
connect_pins pwm/cfg  $config_name/cfg

##########################################################
# Add address module
##########################################################
add_address_module $address_name $bram_addr_width $adc_clk $addr_offset
connect_pins $address_name/clk  $adc_clk
connect_pins $address_name/cfg  $config_name/cfg

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
  cell xilinx.com:ip:xlslice:1.0 dac_${channel}_slice \
    [list                                             \
      DIN_WIDTH 32                                    \
      DIN_FROM [expr $dac_width-1+16*$i]              \
      DIN_TO [expr 16*$i]]                            \
    [list                                             \
      Din blk_mem_gen_$dac_bram_name/doutb            \
      Dout adc_dac/dac/dac_dat_${channel}_i]
}
# Connect remaining ports of BRAM
connect_constant ${dac_bram_name}_dinb 0 32 blk_mem_gen_$dac_bram_name/dinb
connect_constant ${dac_bram_name}_enb  1 1  blk_mem_gen_$dac_bram_name/enb
connect_constant ${dac_bram_name}_web  0 4  blk_mem_gen_$dac_bram_name/web
connect_pins blk_mem_gen_$dac_bram_name/rstb     rst_${ps_name}_125M/peripheral_reset

###########################################################
# Add ADC BRAMs
###########################################################
for {set i 0} {$i < 2} {incr i} {
  set channel [lindex {a b} $i]
  set adc_bram_name adc${i}_bram
  set avg_name      avg$i
  add_bram $adc_bram_name $bram_size
  # Connect port B of BRAM to ADC clock
  connect_constant ${adc_bram_name}_enb 1 1 blk_mem_gen_$adc_bram_name/enb
  connect_pins blk_mem_gen_$adc_bram_name/clkb    $adc_clk
  connect_pins blk_mem_gen_$adc_bram_name/addrb   $address_name/addr_delayed
  connect_pins blk_mem_gen_$adc_bram_name/rstb    rst_${ps_name}_125M/peripheral_reset

  # Add averaging module
  add_averaging_module $avg_name $bram_addr_width $adc_width $adc_clk $avg_offset

  connect_pins $avg_name/start      $address_name/start
  connect_pins $avg_name/cfg        $config_name/cfg
  connect_pins $avg_name/data_in    adc_dac/adc/adc_dat_${channel}_o
  connect_pins $avg_name/addr       $address_name/addr
  connect_pins $avg_name/data_out   blk_mem_gen_$adc_bram_name/dinb
  connect_pins $avg_name/wen        blk_mem_gen_$adc_bram_name/web
}
