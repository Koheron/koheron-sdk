source boards/$board_name/config/ports.tcl

# Add PS and AXI Interconnect
set board_preset boards/$board_name/config/board_preset.tcl
source fpga/lib/starting_point.tcl

# Add ADCs and DACs
source fpga/lib/redp_adc_dac.tcl
set adc_dac_name adc_dac
add_redp_adc_dac $adc_dac_name

# Rename clocks
set adc_clk $adc_dac_name/adc_clk

# Add processor system reset synchronous to adc clock
set rst_adc_clk_name proc_sys_reset_adc_clk
cell xilinx.com:ip:proc_sys_reset:5.0 $rst_adc_clk_name {} {
  ext_reset_in $ps_name/FCLK_RESET0_N
  slowest_sync_clk $adc_clk
}

# Add config and status registers
source fpga/lib/cfg_sts.tcl
add_cfg_sts $adc_clk $rst_adc_clk_name/peripheral_aresetn

# Connect LEDs
connect_port_pin led_o [get_slice_pin [cfg_pin led] 7 0]

# Connect ADC to status register
for {set i 0} {$i < [get_parameter n_adc]} {incr i} {
  connect_pins [sts_pin adc$i] adc_dac/adc[expr $i + 1]
  connect_pins [cfg_pin dac$i] adc_dac/dac[expr $i + 1]
}

# Add XADC for laser current and laser power monitoring
source fpga/lib/xadc.tcl
add_xadc xadc

# Add pulse density modulator for laser current control
cell koheron:user:pdm:1.0 laser_current_pdm {
  NBITS [get_parameter pwm_width]
} {
  clk adc_dac/pwm_clk
  rst $rst_adc_clk_name/peripheral_reset
  din [cfg_pin laser_current]
}
connect_port_pin dac_pwm_o [get_concat_pin [list [get_constant_pin 0 3] laser_current_pdm/dout]]

# Connect laser shutdown pin and reset overvoltage protection
create_bd_port -dir O laser_shutdown
create_bd_port -dir O laser_reset_overvoltage

connect_port_pin laser_shutdown [get_slice_pin [cfg_pin laser_shutdown] 0 0]
connect_port_pin laser_reset_overvoltage [get_slice_pin [cfg_pin laser_reset_overvoltage] 0 0]
