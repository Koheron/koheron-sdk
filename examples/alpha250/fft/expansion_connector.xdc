# ----------------------------------------------------------------------------------
# Expansion connector IOs (Bank 35)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVDS25 [get_ports trigger_*]

set_property PACKAGE_PIN J14 [get_ports trigger_in_n] ;# AD6N
set_property PACKAGE_PIN K14 [get_ports trigger_in_p] ;# AD6P

set_property PACKAGE_PIN L15 [get_ports trigger_out_n] ;# AD7N
set_property PACKAGE_PIN L14 [get_ports trigger_out_p] ;# AD7P

set_property IOSTANDARD LVCMOS33 [get_ports exp_io_*]

set_property PACKAGE_PIN M15 [get_ports exp_io_2_n]
set_property PACKAGE_PIN M14 [get_ports exp_io_2_p]
set_property PACKAGE_PIN J19 [get_ports exp_io_3_n]
set_property PACKAGE_PIN K19 [get_ports exp_io_3_p]
set_property PACKAGE_PIN L20 [get_ports exp_io_4_n] ;# AD3N
set_property PACKAGE_PIN L19 [get_ports exp_io_4_p] ;# AD3P
set_property PACKAGE_PIN H17 [get_ports exp_io_5_n] ;# MRCC
set_property PACKAGE_PIN H16 [get_ports exp_io_5_p] ;# MRCC
set_property PACKAGE_PIN M20 [get_ports exp_io_6_n] ;# AD2N
set_property PACKAGE_PIN M19 [get_ports exp_io_6_p] ;# AD2P
set_property PACKAGE_PIN N16 [get_ports exp_io_7_n] ;# AD14N
set_property PACKAGE_PIN N15 [get_ports exp_io_7_p] ;# AD14P