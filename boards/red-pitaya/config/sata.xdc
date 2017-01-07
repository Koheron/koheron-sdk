### SATA connector

set_property IOSTANDARD DIFF_HSTL_I_18 [get_ports daisy_p_o[*]]
set_property IOSTANDARD DIFF_HSTL_I_18 [get_ports daisy_n_o[*]]

set_property IOSTANDARD DIFF_HSTL_I_18 [get_ports daisy_p_i[*]]
set_property IOSTANDARD DIFF_HSTL_I_18 [get_ports daisy_n_i[*]]

set_property PACKAGE_PIN T12 [get_ports {daisy_p_o[0]}]
set_property PACKAGE_PIN U12 [get_ports {daisy_n_o[0]}]

set_property PACKAGE_PIN U14 [get_ports {daisy_p_o[1]}] ;# MRCC
set_property PACKAGE_PIN U15 [get_ports {daisy_n_o[1]}] ;# MRCC

set_property PACKAGE_PIN P14 [get_ports {daisy_p_i[0]}]
set_property PACKAGE_PIN R14 [get_ports {daisy_n_i[0]}]

set_property PACKAGE_PIN N18 [get_ports {daisy_p_i[1]}] ;# MRCC
set_property PACKAGE_PIN P19 [get_ports {daisy_n_i[1]}] ;# MRCC
