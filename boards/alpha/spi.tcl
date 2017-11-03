# Slow ADC SPI (SPI0)

create_bd_port -dir O spi_slow_adc_cs
create_bd_port -dir O spi_slow_adc_sck
create_bd_port -dir O spi_slow_adc_sdi
create_bd_port -dir I spi_slow_adc_sdo

connect_bd_net [get_bd_ports spi_slow_adc_cs] [get_bd_pins ps_0/SPI0_SS_O]
connect_bd_net [get_bd_ports spi_slow_adc_sck] [get_bd_pins ps_0/SPI0_SCLK_O]
connect_bd_net [get_bd_ports spi_slow_adc_sdi] [get_bd_pins ps_0/SPI0_MOSI_O]
connect_bd_net [get_bd_ports spi_slow_adc_sdo] [get_bd_pins ps_0/SPI0_MISO_I]

#---------------------------------------------------------------------------------------
# Start Slow DAC IP
#---------------------------------------------------------------------------------------

create_bd_port -dir O spi_slow_dac_cs
create_bd_port -dir O spi_slow_dac_sck
create_bd_port -dir O spi_slow_dac_sdi
create_bd_port -dir O spi_slow_dac_ldac

cell koheron:user:slow_dac:1.0 slow_dac_0 {} {
  clk adc_dac/adc_clk
  data [get_concat_pin [list [ctl_pin slow_dac_data0] [ctl_pin slow_dac_data1]] "concat_slow_dac_data"]
  sync spi_slow_dac_cs
  sdi spi_slow_dac_sdi
  sclk spi_slow_dac_sck
  ldac spi_slow_dac_ldac
}

group_bd_cells slow_dac [get_bd_cells slow_dac_0]
set bd [current_bd_instance .]
current_bd_instance slow_dac

create_bd_pin -dir I -from 31 -to 0 ctl

connect_pins slow_dac_0/valid [get_slice_pin ctl 0 0]
connect_pins slow_dac_0/cmd [get_slice_pin ctl 4 1]

current_bd_instance $bd

connect_cell slow_dac {
    ctl [ctl_pin slow_dac_ctl]
}

#---------------------------------------------------------------------------------------
# End Slow DAC IP
#---------------------------------------------------------------------------------------
