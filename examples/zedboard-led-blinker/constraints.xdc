set_property ctlBVS VCCO [current_design]
set_property CONFIG_VOLTAGE 3.3 [current_design]

# ----------------------------------------------------------------------------
# User LEDs - Bank 33
# ----------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS33 [get_ports {led_o[*]}]

set_property PACKAGE_PIN T22 [get_ports {led_o[0]}];  # "LD0"
set_property PACKAGE_PIN T21 [get_ports {led_o[1]}];  # "LD1"
set_property PACKAGE_PIN U22 [get_ports {led_o[2]}];  # "LD2"
set_property PACKAGE_PIN U21 [get_ports {led_o[3]}];  # "LD3"
set_property PACKAGE_PIN V22 [get_ports {led_o[4]}];  # "LD4"
set_property PACKAGE_PIN W22 [get_ports {led_o[5]}];  # "LD5"
set_property PACKAGE_PIN U19 [get_ports {led_o[6]}];  # "LD6"
set_property PACKAGE_PIN U14 [get_ports {led_o[7]}];  # "LD7"

# http://blog.idv-tech.com/2014/03/22/howto-export-zynq-peripheralsi2c-spi-uart-and-etc-to-pmod-connectors-of-zedboard-using-vivado-2013-4/

# SPI routed to PMOD connector using EMIO

set_property IOSTANDARD LVCMOS33 [get_ports {spi_0_*}]

set_property PACKAGE_PIN Y11 [get_ports spi_0_io0_io];  # "JA1"
set_property PACKAGE_PIN AA11 [get_ports spi_0_io1_io];  # "JA2"
set_property PACKAGE_PIN Y10 [get_ports spi_0_sck_io];  # "JA3"
set_property PACKAGE_PIN AA9 [get_ports spi_0_ss1_o];  # "JA4"
set_property PACKAGE_PIN AB11 [get_ports spi_0_ss2_o];  # "JA7"
set_property PACKAGE_PIN AB10 [get_ports spi_0_ss_io];  # "JA8"


# IIC routed to PMOD connector using EMIO

set_property IOSTANDARD LVCMOS33 [get_ports {iic_0_*}]
set_property PULLUP true [get_ports {iic_0_*}]

set_property PACKAGE_PIN W7 [get_ports iic_0_scl_io];  # "JD1_N"
set_property PACKAGE_PIN V7 [get_ports iic_0_sda_io];  # "JD1_P"
