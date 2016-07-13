### EEPROM

create_bd_port -dir I spi_dout
create_bd_port -dir O spi_din
create_bd_port -dir O spi_sclk
create_bd_port -dir O spi_cs

cell koheron:user:at93c46d_spi:1.0 at93c46d_spi_0 {} {
  clk $adc_clk
  data_out [sts_pin spi_out]
}

foreach {name} {
  dout
  din
  sclk
  cs
} {
  connect_bd_net [get_bd_ports spi_$name] [get_bd_pins at93c46d_spi_0/$name]
}

cell xilinx.com:ip:xlslice:1.0 slice_start_eeprom {} {
  Dout at93c46d_spi_0/start
  Din [cfg_pin spi_in]
}

cell xilinx.com:ip:xlslice:1.0 slice_cmd_eeprom {
  DIN_TO 1
  DIN_FROM 8
} {
  Dout at93c46d_spi_0/cmd
  Din [cfg_pin spi_in]
}

cell xilinx.com:ip:xlslice:1.0 slice_data_in_eeprom {
  DIN_TO 16
  DIN_FROM 31
} {
  Dout at93c46d_spi_0/data_in
  Din [cfg_pin spi_in]
}
