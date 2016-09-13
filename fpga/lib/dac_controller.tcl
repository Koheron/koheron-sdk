source $lib/bram.tcl

# Dual DAC controller
# DAC1 and DAC2 values are extracted in paralell from the same 32 bits BRAM register

proc add_dual_dac_controller {module_name memory_name dac_width {intercon_idx 1} {default_hexval 0}} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I -from [expr $config::bram_addr_width + 1] -to 0 addr
  create_bd_pin -dir I -type clk clk
  create_bd_pin -dir I rst

  for {set i 0} {$i < 2} {incr i} {
    create_bd_pin -dir O -from [expr $dac_width - 1] -to 0 dac$i
  }

  add_bram $memory_name $intercon_idx $default_hexval

  connect_cell blk_mem_gen_$memory_name {
    clkb clk
    addrb addr
    rstb rst
    dinb [get_constant_pin 0 32]
    enb  [get_constant_pin 1 1]
    web  [get_constant_pin 0 4]
  }

  # Connect BRAM output to DACs
  for {set i 0} {$i < 2} {incr i} {
    set from [expr $dac_width-1+16*$i]
    set to   [expr 16*$i]
    connect_pins dac$i [get_slice_pin blk_mem_gen_$memory_name/doutb $from $to]
  }

  current_bd_instance $bd

}

# Single DAC controller
# 2 consecutive DAC values are extracted from the same 32 bits BRAM register

proc add_single_dac_controller {module_name memory_name dac_width {intercon_idx 1} {default_hexval 0}} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I -from [expr $config::bram_addr_width + 1] -to 0 addr
  create_bd_pin -dir I -type clk clk
  create_bd_pin -dir I rst

  create_bd_pin -dir O -from [expr $dac_width - 1] -to 0 dac

  add_bram $memory_name $intercon_idx $default_hexval

  connect_cell blk_mem_gen_$memory_name {
    clkb clk
    rstb rst
    dinb [get_constant_pin 0 32]
    enb  [get_constant_pin 1 1]
    web  [get_constant_pin 0 4]
    addrb [get_concat_pin [list \
            [get_constant_pin 0 2] \
            [get_slice_pin addr [expr $config::bram_addr_width + 1] 3]]]
  }

  cell koheron:user:bus_multiplexer:1.0 mux {
    WIDTH $dac_width
  } {
    sel [get_slice_pin addr 2 2]
    in0 [get_slice_pin blk_mem_gen_$memory_name/doutb [expr $dac_width-1] 0]
    in1 [get_slice_pin blk_mem_gen_$memory_name/doutb [expr $dac_width-1 + 16] 16]
    out dac
  }

  current_bd_instance $bd

}
