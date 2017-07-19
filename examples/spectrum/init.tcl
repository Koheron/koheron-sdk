source $board_path/config/ports.tcl
source $board_path/base_system.tcl

source $sdk_path/fpga/lib/laser_controller.tcl

# Add address module
source $sdk_path/fpga/modules/address/address.tcl
set address_name address

address::create $address_name [expr [get_memory_addr_width dac0] + 1] [get_parameter n_dac]

connect_cell $address_name {
  clk  $adc_clk
  cfg  [ctl_pin addr]
}

# Add Dac controllers
source $sdk_path/fpga/lib/dac_controller.tcl

for {set i 0} {$i < [get_parameter n_dac]} {incr i} {

  connect_pins $address_name/period$i  [ctl_pin dac_period$i]

  set dac_controller$i dac${i}_ctrl
  add_single_dac_controller dac_controller$i dac$i [get_parameter dac_width] 1
  connect_cell dac_controller$i {
    clk  $adc_clk
    addr $address_name/addr$i
    rst  $rst_adc_clk_name/peripheral_reset
    dac  $adc_dac_name/dac[expr $i+1]
  }
}
