set_property CFGBVS GND [current_design]
set_property CONFIG_VOLTAGE 1.8 [current_design]

# ----------------------------------------------------------------------------
# User LEDs
# ----------------------------------------------------------------------------

set_property IOSTANDARD	LVCMOS18 [get_ports {led_o[*]}]

set_property PACKAGE_PIN A17 [get_ports {led_o[0]}];  # "LD0"
set_property PACKAGE_PIN W21  [get_ports {led_o[1]}];  # "LD1"
set_property PACKAGE_PIN G2 [get_ports {led_o[2]}];  # "LD2"
set_property PACKAGE_PIN Y21 [get_ports {led_o[3]}];  # "LD3"