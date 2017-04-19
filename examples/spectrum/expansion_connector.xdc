### Expansion connector

set_property IOSTANDARD LVCMOS33 [get_ports spi_*]
set_property SLEW FAST [get_ports spi_*]
set_property IOSTANDARD LVCMOS33 [get_ports spi_*]

set_property PACKAGE_PIN G18 [get_ports spi_dout]
set_property PACKAGE_PIN H16 [get_ports spi_din]
set_property PACKAGE_PIN H17 [get_ports spi_sclk]
set_property PACKAGE_PIN J18 [get_ports spi_cs]

set_property IOSTANDARD LVCMOS33 [get_ports laser_*]
set_property SLEW FAST [get_ports laser_*]

set_property PACKAGE_PIN M14 [get_ports laser_shutdown] ;# DIO7_P
set_property PACKAGE_PIN M15 [get_ports laser_reset_overvoltage] ;# DIO7_N