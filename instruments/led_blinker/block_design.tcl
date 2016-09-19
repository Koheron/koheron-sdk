# Add PS and AXI Interconnect
set board_preset boards/$board_name/config/board_preset.tcl
source fpga/lib/starting_point.tcl

# Add config and status registers
source fpga/lib/cfg_sts.tcl
add_cfg_sts

# Connect LEDs to config register
create_bd_port -dir O -from 7 -to 0 led_o
connect_port_pin led_o [get_slice_pin [cfg_pin led] 7 0]]

# Connect 42 to status register
connect_pins [get_constant_pin 42 32] [sts_pin forty_two]
