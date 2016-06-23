source $lib/bram.tcl

# Single BRAM recorder (32 bit width)

proc add_bram_recorder {module_name bram_name} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I -from [expr $config::bram_addr_width + 1] -to 0 addr
  create_bd_pin -dir I -from 31 -to 0 adc
  create_bd_pin -dir I -from 3  -to 0 wen
  create_bd_pin -dir I -type clk clk
  create_bd_pin -dir I rst

  add_bram $bram_name [set config::axi_${bram_name}_range] [set config::axi_${bram_name}_offset]

  connect_cell blk_mem_gen_$bram_name {
    addrb addr
    dinb adc
    clkb clk
    rstb rst
    web wen
    enb [get_constant_pin 1 1]
  }

  current_bd_instance $bd

}
