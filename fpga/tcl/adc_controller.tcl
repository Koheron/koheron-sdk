source $lib/bram.tcl

# Single ADC controller (32 bit width)

proc add_adc_controller {module_name bram_name} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I -from [expr $config::bram_addr_width + 1] -to 0 addr
  create_bd_pin -dir I -type clk clk
  create_bd_pin -dir I rst

  create_bd_pin -dir I -from 31 -to 0 adc
  create_bd_pin -dir I -from 3  -to 0 wen

  set bram_name adc_bram
  add_bram $bram_name [set config::axi_${bram_name}_range] [set config::axi_${bram_name}_offset]

  connect_constant ${bram_name}_enb  1 1  blk_mem_gen_$bram_name/enb

  connect_pins blk_mem_gen_$bram_name/dinb adc
  connect_pins addr blk_mem_gen_$bram_name/addrb
  connect_pins clk  blk_mem_gen_$bram_name/clkb
  connect_pins rst  blk_mem_gen_$bram_name/rstb
  connect_pins wen  blk_mem_gen_$bram_name/web

  current_bd_instance $bd

}
