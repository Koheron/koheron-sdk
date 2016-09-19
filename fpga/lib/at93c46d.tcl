### EEPROM

create_bd_port -dir I spi_dout
create_bd_port -dir O spi_din
create_bd_port -dir O spi_sclk
create_bd_port -dir O spi_cs

cell koheron:user:at93c46d_spi:1.0 at93c46d_spi_0 {} {
  clk      $adc_clk
  data_out [sts_pin spi_out]
  start    [get_slice_pin [cfg_pin spi_in] 0 0]
  cmd      [get_slice_pin [cfg_pin spi_in] 8 1]
  data_in  [get_slice_pin [cfg_pin spi_in] 31 16]
}

foreach {name} {
  dout
  din
  sclk
  cs
} {
  connect_port_pin spi_$name at93c46d_spi_0/$name
}
