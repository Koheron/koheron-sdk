set_property CFGBVS VCCO [current_design]
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
