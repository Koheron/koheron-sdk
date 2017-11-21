# Add PS and AXI Interconnect
set board_preset $board_path/config/board_preset.tcl

source $sdk_path/fpga/lib/starting_point.tcl
#source $sdk_path/fpga/lib/starting_point.tcl

# Add config and status registers
source $sdk_path/fpga/lib/ctl_sts.tcl
add_ctl_sts

# Connect LEDs to config register
create_bd_port -dir O -from 7 -to 0 led_o
connect_port_pin led_o [get_slice_pin [ctl_pin led] 7 0]

# Connect 42 to status register
connect_pins [get_constant_pin 42 32] [sts_pin forty_two]
