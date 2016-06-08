source $lib/bram.tcl

proc add_dac_controller {module_name bram_name bram_size dac_width} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I -from 31 -to 0 addr
  create_bd_pin -dir I -type clk clk
  create_bd_pin -dir I rst

  for {set i 0} {$i < 2} {incr i} {
    create_bd_pin -dir O -from [expr $dac_width - 1] -to 0 dac$i
  }

  add_bram $bram_name [set config::axi_${bram_name}_range] [set config::axi_${bram_name}_offset]

  connect_constant ${bram_name}_dinb 0 32 blk_mem_gen_$bram_name/dinb
  connect_constant ${bram_name}_enb  1 1  blk_mem_gen_$bram_name/enb
  connect_constant ${bram_name}_web  0 4  blk_mem_gen_$bram_name/web

  connect_pins addr blk_mem_gen_$bram_name/addrb
  connect_pins rst  blk_mem_gen_$bram_name/rstb

  # Connect BRAM output to DACs
  for {set i 0} {$i < 2} {incr i} {
    cell xilinx.com:ip:xlslice:1.0 dac${i}_slice {
      DIN_WIDTH 32
      DIN_FROM [expr $dac_width-1+16*$i]
      DIN_TO [expr 16*$i]
    } {
      Din blk_mem_gen_$bram_name/doutb
      Dout dac$i
    }
  }

  current_bd_instance $bd

}
