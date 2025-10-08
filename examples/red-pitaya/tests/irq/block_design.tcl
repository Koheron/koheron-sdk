
set ps_name ps_0
create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7:5.5 $ps_name

source $board_path/config/board_preset.tcl

create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 proc_sys_reset_0

set_property CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {100.0} [get_bd_cells ps_0]

apply_bd_automation -rule xilinx.com:bd_rule:processing_system7 -config {make_external "FIXED_IO, DDR" Master "Disable" Slave "Disable" }  [get_bd_cells ps_0]

apply_bd_automation -rule xilinx.com:bd_rule:board -config { Manual_Source {Auto}}  [get_bd_pins proc_sys_reset_0/ext_reset_in]
apply_bd_automation -rule xilinx.com:bd_rule:clkrst -config { Clk {/ps_0/FCLK_CLK0 (100 MHz)} Freq {100} Ref_Clk0 {} Ref_Clk1 {} Ref_Clk2 {}}  [get_bd_pins proc_sys_reset_0/slowest_sync_clk]

create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 smartconnect_0
set_property CONFIG.NUM_SI {1} [get_bd_cells smartconnect_0]

create_bd_cell -type ip -vlnv xilinx.com:ip:axi_timer:2.0 axi_timer_0
set_property -dict [list \
  CONFIG.enable_timer2 {0} \
  CONFIG.mode_64bit {0} \
] [get_bd_cells axi_timer_0]

create_bd_cell -type inline_hdl -vlnv xilinx.com:inline_hdl:ilconcat:1.0 ilconcat_0
connect_bd_net [get_bd_pins axi_timer_0/interrupt] [get_bd_pins ilconcat_0/In0]
connect_bd_net [get_bd_pins ilconcat_0/dout] [get_bd_pins ps_0/IRQ_F2P]

apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} Master {/ps_0/M_AXI_GP0} Slave {/axi_timer_0/S_AXI} ddr_seg {Auto} intc_ip {/smartconnect_0} master_apm {0}}  [get_bd_intf_pins axi_timer_0/S_AXI]

set_property range [get_memory_range timer] [get_bd_addr_segs {ps_0/Data/SEG_axi_timer_0_Reg}]
set_property offset [get_memory_offset timer] [get_bd_addr_segs {ps_0/Data/SEG_axi_timer_0_Reg}]