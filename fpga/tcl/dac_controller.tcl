proc add_dac_controller {module_name bram_name bram_size} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  add_bram $dac_bram_name [set config::axi_${bram_name}_range] [set config::axi_${bram_name}_offset 00

  current_bd_instance $bd

}
