
set ps_name ps_0
create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7:5.5 $ps_name

source $board_path/config/board_preset.tcl

create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 proc_sys_reset_0

set_property CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {100.0} [get_bd_cells ps_0]
set_property CONFIG.PCW_FPGA1_PERIPHERAL_FREQMHZ {50.0} [get_bd_cells ps_0]

apply_bd_automation -rule xilinx.com:bd_rule:processing_system7 -config {make_external "FIXED_IO, DDR" Master "Disable" Slave "Disable" }  [get_bd_cells ps_0]

apply_bd_automation -rule xilinx.com:bd_rule:board -config { Manual_Source {Auto}}  [get_bd_pins proc_sys_reset_0/ext_reset_in]
apply_bd_automation -rule xilinx.com:bd_rule:clkrst -config { Clk {/ps_0/FCLK_CLK0 (100 MHz)} Freq {100} Ref_Clk0 {} Ref_Clk1 {} Ref_Clk2 {}}  [get_bd_pins proc_sys_reset_0/slowest_sync_clk]

create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 smartconnect_0
set_property CONFIG.NUM_SI {1} [get_bd_cells smartconnect_0]


create_bd_cell -type ip -vlnv xilinx.com:ip:axi_fifo_mm_s:4.3 axi_fifo_mm_s_0

set_property -dict [list \
  CONFIG.C_RX_FIFO_DEPTH {16384} \
  CONFIG.C_RX_FIFO_PE_THRESHOLD {5} \
  CONFIG.C_RX_FIFO_PF_THRESHOLD {8192} \
  CONFIG.C_USE_RX_CUT_THROUGH {true} \
  CONFIG.C_USE_TX_CTRL {0} \
  CONFIG.C_USE_TX_DATA {0} \
] [get_bd_cells axi_fifo_mm_s_0]

create_bd_cell -type inline_hdl -vlnv xilinx.com:inline_hdl:ilconcat:1.0 ilconcat_0
connect_bd_net [get_bd_pins axi_fifo_mm_s_0/interrupt] [get_bd_pins ilconcat_0/In0]
connect_bd_net [get_bd_pins ilconcat_0/dout] [get_bd_pins ps_0/IRQ_F2P]

create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_0
set_property CONFIG.Output_Width {32} [get_bd_cells c_counter_binary_0]
connect_bd_net [get_bd_pins ps_0/FCLK_CLK1] [get_bd_pins c_counter_binary_0/CLK]

create_bd_cell -type ip -vlnv xilinx.com:ip:axis_clock_converter:1.1 axis_clock_converter_0
set_property CONFIG.TDATA_NUM_BYTES {4} [get_bd_cells axis_clock_converter_0]
set_property CONFIG.HAS_TLAST {1} [get_bd_cells axis_clock_converter_0]
connect_bd_net [get_bd_pins axis_clock_converter_0/s_axis_tdata] [get_bd_pins c_counter_binary_0/Q]
create_bd_cell -type inline_hdl -vlnv xilinx.com:inline_hdl:ilconstant:1.0 ilconstant_0
connect_bd_net [get_bd_pins ilconstant_0/dout] [get_bd_pins axis_clock_converter_0/s_axis_tvalid]
connect_bd_net [get_bd_pins ilconstant_0/dout] [get_bd_pins axis_clock_converter_0/s_axis_tlast]
connect_bd_net [get_bd_pins axis_clock_converter_0/s_axis_aclk] [get_bd_pins ps_0/FCLK_CLK1]
connect_bd_net [get_bd_pins axis_clock_converter_0/m_axis_aclk] [get_bd_pins ps_0/FCLK_CLK0]
connect_bd_net [get_bd_pins axis_clock_converter_0/m_axis_aresetn] [get_bd_pins proc_sys_reset_0/peripheral_aresetn]
connect_bd_net [get_bd_pins axis_clock_converter_0/s_axis_aresetn] [get_bd_pins proc_sys_reset_0/peripheral_aresetn]
connect_bd_intf_net [get_bd_intf_pins axis_clock_converter_0/M_AXIS] [get_bd_intf_pins axi_fifo_mm_s_0/AXI_STR_RXD]

apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} Master {/ps_0/M_AXI_GP0} Slave {/axi_fifo_mm_s_0/S_AXI} ddr_seg {Auto} intc_ip {/smartconnect_0} master_apm {0}}  [get_bd_intf_pins axi_fifo_mm_s_0/S_AXI]

set_property offset [get_memory_offset fifo] [get_bd_addr_segs {ps_0/Data/SEG_axi_fifo_mm_s_0_Mem0}]
set_property range [get_memory_range fifo] [get_bd_addr_segs {ps_0/Data/SEG_axi_fifo_mm_s_0_Mem0}]