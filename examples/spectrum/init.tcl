source $board_path/config/ports.tcl
source $board_path/base_system.tcl

# Add pulse density modulator for laser current control
cell koheron:user:pdm:1.0 laser_current_pdm {
  NBITS [get_parameter pwm_width]
} {
  clk adc_dac/pwm_clk
  rst $rst_adc_clk_name/peripheral_reset
  din [ctl_pin laser_current]
}
connect_port_pin dac_pwm_o [get_concat_pin [list [get_constant_pin 0 3] laser_current_pdm/dout]]

# Connect laser shutdown pin and reset overvoltage protection
create_bd_port -dir O laser_shutdown
create_bd_port -dir O laser_reset_overvoltage

connect_port_pin laser_shutdown [get_slice_pin [ctl_pin laser_shutdown] 0 0]
connect_port_pin laser_reset_overvoltage [get_slice_pin [ctl_pin laser_reset_overvoltage] 0 0]

# Add address module
source $module_path/address/address.tcl
set address_name address

address::create $address_name [expr [get_memory_addr_width dac0] + 1] [get_parameter n_dac]

connect_cell $address_name {
  clk  $adc_clk
  cfg  [ctl_pin addr]
}

# Add Dac controllers
source $lib_path/dac_controller.tcl

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
