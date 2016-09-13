source $lib/bram.tcl

# Single BRAM recorder (32 bit width)

proc add_bram_recorder {module_name memory_name} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I -from [expr $config::bram_addr_width + 1] -to 0 addr
  create_bd_pin -dir I -from 31 -to 0 adc
  create_bd_pin -dir I -from 3  -to 0 wen
  create_bd_pin -dir I -type clk clk
  create_bd_pin -dir I rst

  set bram_name [add_bram $memory_name]

  connect_cell $bram_name {
    addrb addr
    dinb adc
    clkb clk
    rstb rst
    web wen
    enb [get_constant_pin 1 1]
  }

  current_bd_instance $bd

}
