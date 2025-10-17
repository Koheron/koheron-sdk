# Basic processing system and register infrastructure for the KR260
set board_preset $board_path/config/board_preset.tcl
source $sdk_path/fpga/lib/starting_point_zynqmp.tcl

# Add control and status register banks clocked from pl_clk0
source $sdk_path/fpga/lib/ctl_sts.tcl
add_ctl_sts $ps_name/pl_clk0 proc_sys_reset_0/peripheral_aresetn

