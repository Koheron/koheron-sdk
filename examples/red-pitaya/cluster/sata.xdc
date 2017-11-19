### SATA connector

set_property IOSTANDARD DIFF_HSTL_I_18 [get_ports sata_*]

set_property PACKAGE_PIN T12 [get_ports sata_data_out_p]
set_property PACKAGE_PIN U12 [get_ports sata_data_out_n]

set_property PACKAGE_PIN U14 [get_ports sata_clk_out_clk_p] ;# MRCC
set_property PACKAGE_PIN U15 [get_ports sata_clk_out_clk_n] ;# MRCC

set_property PACKAGE_PIN P14 [get_ports sata_data_in_p]
set_property PACKAGE_PIN R14 [get_ports sata_data_in_n]

set_property PACKAGE_PIN N18 [get_ports sata_clk_in_clk_p] ;# MRCC
set_property PACKAGE_PIN P19 [get_ports sata_clk_in_clk_n] ;# MRCC
