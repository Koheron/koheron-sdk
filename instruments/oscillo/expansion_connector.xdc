### Expansion connector

set_property IOSTANDARD LVCMOS33 [get_ports spi_*]
set_property SLEW FAST [get_ports spi_*]
set_property IOSTANDARD LVCMOS33 [get_ports spi_*]

set_property PACKAGE_PIN G18 [get_ports spi_dout]
set_property PACKAGE_PIN H16 [get_ports spi_din]
set_property PACKAGE_PIN H17 [get_ports spi_sclk]
set_property PACKAGE_PIN J18 [get_ports spi_cs]


set_property IOSTANDARD LVCMOS33 [get_ports {exp_p_tri_io[*]}]
set_property IOSTANDARD LVCMOS33 [get_ports {exp_n_tri_io[*]}]
set_property SLEW FAST [get_ports {exp_p_tri_io[*]}]
set_property SLEW FAST [get_ports {exp_n_tri_io[*]}]
set_property DRIVE 8 [get_ports {exp_p_tri_io[*]}]
set_property DRIVE 8 [get_ports {exp_n_tri_io[*]}]

set_property PACKAGE_PIN G17 [get_ports {exp_p_tri_io[0]}]
set_property PACKAGE_PIN H18 [get_ports {exp_n_tri_io[0]}]
set_property PACKAGE_PIN K17 [get_ports {exp_p_tri_io[1]}]
set_property PACKAGE_PIN K18 [get_ports {exp_n_tri_io[1]}]
set_property PACKAGE_PIN L14 [get_ports {exp_p_tri_io[2]}]
set_property PACKAGE_PIN L15 [get_ports {exp_n_tri_io[2]}]
set_property PACKAGE_PIN L16 [get_ports {exp_p_tri_io[3]}]
set_property PACKAGE_PIN L17 [get_ports {exp_n_tri_io[3]}]
set_property PACKAGE_PIN K16 [get_ports {exp_p_tri_io[4]}]
set_property PACKAGE_PIN J16 [get_ports {exp_n_tri_io[4]}]
set_property PACKAGE_PIN M14 [get_ports {exp_p_tri_io[5]}]
set_property PACKAGE_PIN M15 [get_ports {exp_n_tri_io[5]}]
