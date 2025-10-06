
set ps_name ps_0
create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7:5.5 $ps_name

source $board_path/config/board_preset.tcl

create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 proc_sys_reset_0

set_property CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {187.5} [get_bd_cells ps_0]

apply_bd_automation -rule xilinx.com:bd_rule:processing_system7 -config {make_external "FIXED_IO, DDR" Master "Disable" Slave "Disable" }  [get_bd_cells ps_0]

apply_bd_automation -rule xilinx.com:bd_rule:board -config { Manual_Source {Auto}}  [get_bd_pins proc_sys_reset_0/ext_reset_in]
apply_bd_automation -rule xilinx.com:bd_rule:clkrst -config { Clk {/ps_0/FCLK_CLK0 (200 MHz)} Freq {200} Ref_Clk0 {} Ref_Clk1 {} Ref_Clk2 {}}  [get_bd_pins proc_sys_reset_0/slowest_sync_clk]

create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 smartconnect_0
set_property CONFIG.NUM_SI {1} [get_bd_cells smartconnect_0]

create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.4 blk_mem_gen_0

create_bd_cell -type ip -vlnv xilinx.com:ip:axi_bram_ctrl:4.1 axi_bram_ctrl_0
set_property CONFIG.SINGLE_PORT_BRAM {1} [get_bd_cells axi_bram_ctrl_0]
set_property CONFIG.PROTOCOL {AXI4LITE} [get_bd_cells axi_bram_ctrl_0]
connect_bd_intf_net [get_bd_intf_pins axi_bram_ctrl_0/BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_0/BRAM_PORTA]

create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.4 blk_mem_gen_1

create_bd_cell -type ip -vlnv xilinx.com:ip:axi_bram_ctrl:4.1 axi_bram_ctrl_1
set_property CONFIG.SINGLE_PORT_BRAM {1} [get_bd_cells axi_bram_ctrl_1]
connect_bd_intf_net [get_bd_intf_pins axi_bram_ctrl_1/BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_1/BRAM_PORTA]

apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} Master {/ps_0/M_AXI_GP0} Slave {/axi_bram_ctrl_0/S_AXI} ddr_seg {Auto} intc_ip {/smartconnect_0} master_apm {0}}  [get_bd_intf_pins axi_bram_ctrl_0/S_AXI]

apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} Master {/ps_0/M_AXI_GP0} Slave {/axi_bram_ctrl_1/S_AXI} ddr_seg {Auto} intc_ip {/smartconnect_0} master_apm {0}}  [get_bd_intf_pins axi_bram_ctrl_1/S_AXI]

set_property range 64K [get_bd_addr_segs {ps_0/Data/SEG_axi_bram_ctrl_0_Mem0}]
set_property range 64K [get_bd_addr_segs {ps_0/Data/SEG_axi_bram_ctrl_1_Mem0}]