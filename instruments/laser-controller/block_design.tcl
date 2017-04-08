source $board_path/config/ports.tcl

# Add PS and AXI Interconnect
set board_preset $board_path/config/board_preset.tcl
source $lib_path/starting_point.tcl

# Add ADCs and DACs
source $lib_path/redp_adc_dac.tcl
set adc_dac_name adc_dac
add_redp_adc_dac $adc_dac_name

# Rename clocks
set adc_clk $adc_dac_name/adc_clk

# Add processor system reset synchronous to adc clock
set rst_adc_clk_name proc_sys_reset_adc_clk
cell xilinx.com:ip:proc_sys_reset:5.0 $rst_adc_clk_name {} {
  ext_reset_in ps_0/FCLK_RESET0_N
  slowest_sync_clk $adc_clk
}

# Add config and status registers
source $lib_path/ctl_sts.tcl
add_ctl_sts $adc_clk $rst_adc_clk_name/peripheral_aresetn

# Connect LEDs
connect_port_pin led_o [get_slice_pin [ctl_pin led] 7 0]

# Connect EEPROM to SPI_0
create_bd_port -dir O spi_cs
create_bd_port -dir O spi_sclk
create_bd_port -dir O spi_din
create_bd_port -dir I spi_dout

connect_port_pin spi_cs ps_0/SPI0_SS_O
connect_port_pin spi_sclk ps_0/SPI0_SCLK_O
connect_port_pin spi_din ps_0/SPI0_MOSI_O
connect_port_pin spi_dout ps_0/SPI0_MISO_I

# Connect ADC to status register
for {set i 0} {$i < [get_parameter n_adc]} {incr i} {
  connect_pins [sts_pin adc$i] adc_dac/adc[expr $i + 1]
  connect_pins [ctl_pin dac$i] adc_dac/dac[expr $i + 1]
}

# Add XADC for laser current and laser power monitoring
source $lib_path/xadc.tcl
add_xadc xadc

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
