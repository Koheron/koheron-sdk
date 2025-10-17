set ps_name ps_0
create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e:3.5 $ps_name

source $board_path/config/board_preset.tcl

create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 proc_sys_reset_0
apply_bd_automation -rule xilinx.com:bd_rule:board -config { Manual_Source {/ps_0/pl_resetn0 (ACTIVE_LOW)}}  [get_bd_pins proc_sys_reset_0/ext_reset_in]
apply_bd_automation -rule xilinx.com:bd_rule:clkrst -config { Clk {/ps_0/pl_clk0 (99 MHz)} Freq {99} Ref_Clk0 {None} Ref_Clk1 {None} Ref_Clk2 {None}}  [get_bd_pins proc_sys_reset_0/slowest_sync_clk]

create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio:2.0 axi_gpio_0
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {/ps_0/pl_clk0 (99 MHz)} Clk_slave {/ps_0/pl_clk0 (99 MHz)} Clk_xbar {/ps_0/pl_clk0 (99 MHz)} Master {/ps_0/M_AXI_HPM0_FPD} Slave {/axi_gpio_0/S_AXI} ddr_seg {Auto} intc_ip {New AXI SmartConnect} master_apm {0}}  [get_bd_intf_pins axi_gpio_0/S_AXI]

set_property range [get_memory_range gpio] [get_bd_addr_segs {ps_0/Data/SEG_axi_gpio_0_Reg}]
set_property offset [get_memory_offset gpio] [get_bd_addr_segs {ps_0/Data/SEG_axi_gpio_0_Reg}]

connect_bd_net [get_bd_pins ps_0/dp_audio_ref_clk] [get_bd_pins ps_0/dp_s_axis_audio_clk]
