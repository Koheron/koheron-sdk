### EEPROM

create_bd_port -dir I spi_dout
create_bd_port -dir O spi_din
create_bd_port -dir O spi_sclk
create_bd_port -dir O spi_cs

create_bd_cell -type ip -vlnv koheron:user:at93c46d_spi:1.0 at93c46d_spi_0

connect_bd_net [get_bd_ports spi_dout] [get_bd_pins at93c46d_spi_0/dout]
connect_bd_net [get_bd_ports spi_din] [get_bd_pins at93c46d_spi_0/din]
connect_bd_net [get_bd_ports spi_sclk] [get_bd_pins at93c46d_spi_0/sclk]
connect_bd_net [get_bd_ports spi_cs] [get_bd_pins at93c46d_spi_0/cs]

connect_pins at93c46d_spi_0/clk $adc_clk

create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 slice_start_eeprom
connect_pins slice_start_eeprom/Dout at93c46d_spi_0/start
connect_pins slice_start_eeprom/Din $config_name/Out8

create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 slice_cmd_eeprom
set_property -dict [list CONFIG.DIN_TO {1} CONFIG.DIN_FROM {8}] [get_bd_cells slice_cmd_eeprom]
connect_pins slice_cmd_eeprom/Dout at93c46d_spi_0/cmd
connect_pins slice_cmd_eeprom/Din $config_name/Out8

create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 slice_data_in_eeprom
set_property -dict [list CONFIG.DIN_TO {16} CONFIG.DIN_FROM {31}] [get_bd_cells slice_data_in_eeprom]
connect_pins slice_data_in_eeprom/Dout at93c46d_spi_0/data_in
connect_pins slice_data_in_eeprom/Din $config_name/Out8

connect_pins at93c46d_spi_0/data_out $status_name/In2
