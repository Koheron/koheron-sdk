### Expansion connector

set_property IOSTANDARD LVCMOS33 [get_ports laser_*]
set_property SLEW FAST [get_ports laser_*]

set_property PACKAGE_PIN M14 [get_ports laser_shutdown] ;# DIO7_P
set_property PACKAGE_PIN M15 [get_ports laser_reset_overvoltage] ;# DIO7_N
