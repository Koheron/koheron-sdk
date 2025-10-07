
set ps_name ps_0
create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7:5.5 $ps_name

source $board_path/config/board_preset.tcl

create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 proc_sys_reset_0

set_property CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {200.0} [get_bd_cells ps_0]

apply_bd_automation -rule xilinx.com:bd_rule:processing_system7 -config {make_external "FIXED_IO, DDR" Master "Disable" Slave "Disable" }  [get_bd_cells ps_0]

apply_bd_automation -rule xilinx.com:bd_rule:board -config { Manual_Source {Auto}}  [get_bd_pins proc_sys_reset_0/ext_reset_in]
apply_bd_automation -rule xilinx.com:bd_rule:clkrst -config { Clk {/ps_0/FCLK_CLK0 (200 MHz)} Freq {200} Ref_Clk0 {} Ref_Clk1 {} Ref_Clk2 {}}  [get_bd_pins proc_sys_reset_0/slowest_sync_clk]

create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 smartconnect_0
set_property CONFIG.NUM_SI {1} [get_bd_cells smartconnect_0]

create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio:2.0 axi_gpio_0
set_property CONFIG.C_ALL_INPUTS {1} [get_bd_cells axi_gpio_0]

create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0
set_property CONFIG.Output_Width {32} [get_bd_cells c_counter_binary_0]

connect_bd_net [get_bd_pins c_counter_binary_0/CLK] [get_bd_pins ps_0/FCLK_CLK0]
connect_bd_net [get_bd_pins c_counter_binary_0/Q] [get_bd_pins axi_gpio_0/gpio_io_i]

apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {/ps_0/FCLK_CLK0 (200 MHz)} Clk_slave {/ps_0/FCLK_CLK0 (200 MHz)} Clk_xbar {/ps_0/FCLK_CLK0 (200 MHz)} Master {/ps_0/M_AXI_GP0} Slave {/axi_gpio_0/S_AXI} ddr_seg {Auto} intc_ip {/smartconnect_0} master_apm {0}}  [get_bd_intf_pins axi_gpio_0/S_AXI]

set_property range [get_memory_range gpio] [get_bd_addr_segs {ps_0/Data/SEG_axi_gpio_0_Reg}]
set_property offset [get_memory_offset gpio] [get_bd_addr_segs {ps_0/Data/SEG_axi_gpio_0_Reg}]