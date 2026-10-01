
set ps_name ps_0
create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7:5.5 $ps_name

source $board_path/config/board_preset.tcl

create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 proc_sys_reset_0

set_property -dict [list \
  CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {100.0} \
  CONFIG.PCW_FPGA1_PERIPHERAL_FREQMHZ {50.0} \
  CONFIG.PCW_EN_CLK2_PORT {1} \
  CONFIG.PCW_FPGA2_PERIPHERAL_FREQMHZ {50.0} \
  CONFIG.PCW_USE_S_AXI_HP0 {1} \
  CONFIG.PCW_S_AXI_HP0_DATA_WIDTH {32}
] [get_bd_cells ps_0]

apply_bd_automation -rule xilinx.com:bd_rule:processing_system7 -config {make_external "FIXED_IO, DDR" Master "Disable" Slave "Disable" }  [get_bd_cells ps_0]

apply_bd_automation -rule xilinx.com:bd_rule:board -config { Manual_Source {Auto}}  [get_bd_pins proc_sys_reset_0/ext_reset_in]
apply_bd_automation -rule xilinx.com:bd_rule:clkrst -config { Clk {/ps_0/FCLK_CLK0 (100 MHz)} Freq {100} Ref_Clk0 {} Ref_Clk1 {} Ref_Clk2 {}}  [get_bd_pins proc_sys_reset_0/slowest_sync_clk]

create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 smartconnect_0
set_property CONFIG.NUM_SI {1} [get_bd_cells smartconnect_0]
set_property CONFIG.NUM_MI {2} [get_bd_cells smartconnect_0]
connect_bd_intf_net [get_bd_intf_pins smartconnect_0/S00_AXI] [get_bd_intf_pins ps_0/M_AXI_GP0]
connect_bd_net [get_bd_pins smartconnect_0/aclk] [get_bd_pins ps_0/FCLK_CLK0]
connect_bd_net [get_bd_pins ps_0/M_AXI_GP0_ACLK] [get_bd_pins ps_0/FCLK_CLK0]
connect_bd_net [get_bd_pins proc_sys_reset_0/peripheral_aresetn] [get_bd_pins smartconnect_0/aresetn]

create_bd_cell -type inline_hdl -vlnv xilinx.com:inline_hdl:ilconcat:1.0 ilconcat_0
set_property CONFIG.NUM_PORTS {3} [get_bd_cells ilconcat_0]
connect_bd_net [get_bd_pins ilconcat_0/dout] [get_bd_pins ps_0/IRQ_F2P]

create_bd_cell -type inline_hdl -vlnv xilinx.com:inline_hdl:ilconstant:1.0 ilconstant_0

create_bd_cell -type ip -vlnv koheron:user:axis_stream_packet_mux:1.0 axis_stream_packet_m_0
connect_bd_intf_net [get_bd_intf_pins smartconnect_0/M01_AXI] [get_bd_intf_pins axis_stream_packet_m_0/S_AXI_LITE]
connect_bd_net [get_bd_pins axis_stream_packet_m_0/aclk] [get_bd_pins ps_0/FCLK_CLK0]
connect_bd_net [get_bd_pins axis_stream_packet_m_0/aresetn] [get_bd_pins proc_sys_reset_0/peripheral_aresetn]

for {set i 0} {$i < 2} {incr i} {

  create_bd_cell -type ip -vlnv xilinx.com:ip:c_counter_binary:12.0 c_counter_binary_$i
  set_property CONFIG.Output_Width {32} [get_bd_cells c_counter_binary_$i]
  connect_bd_net [get_bd_pins ps_0/FCLK_CLK[expr $i+1]] [get_bd_pins c_counter_binary_$i/CLK]

  create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 axis_data_fifo_$i
  set_property -dict [list \
    CONFIG.FIFO_DEPTH {16384} \
    CONFIG.TDATA_NUM_BYTES {4} \
    CONFIG.IS_ACLK_ASYNC {1} \
    CONFIG.HAS_PROG_FULL {1} \
    CONFIG.PROG_FULL_THRESH {8192} \
  ] [get_bd_cells axis_data_fifo_$i]

  connect_bd_net [get_bd_pins ilconcat_0/In$i] [get_bd_pins axis_data_fifo_$i/prog_full]
  connect_bd_net [get_bd_pins ilconstant_0/dout] [get_bd_pins axis_data_fifo_$i/s_axis_tvalid]
  connect_bd_net [get_bd_pins axis_data_fifo_$i/s_axis_aclk] [get_bd_pins ps_0/FCLK_CLK[expr $i+1]]
  connect_bd_net [get_bd_pins axis_data_fifo_$i/s_axis_aresetn] [get_bd_pins proc_sys_reset_0/peripheral_aresetn]
  connect_bd_net [get_bd_pins axis_data_fifo_$i/m_axis_aclk] [get_bd_pins ps_0/FCLK_CLK0]
  connect_bd_net [get_bd_pins axis_data_fifo_$i/s_axis_tdata] [get_bd_pins c_counter_binary_$i/Q]
  connect_bd_intf_net [get_bd_intf_pins axis_stream_packet_m_0/S_AXIS_$i] [get_bd_intf_pins axis_data_fifo_$i/M_AXIS]

}

create_bd_cell -type ip -vlnv xilinx.com:ip:axi_dma:7.1 axi_dma_0

set_property -dict [list \
  CONFIG.c_include_mm2s {0} \
  CONFIG.c_include_sg {0} \
  CONFIG.c_s2mm_burst_size {64} \
] [get_bd_cells axi_dma_0]

connect_bd_intf_net [get_bd_intf_pins smartconnect_0/M00_AXI] [get_bd_intf_pins axi_dma_0/S_AXI_LITE]
connect_bd_intf_net [get_bd_intf_pins axis_stream_packet_m_0/M_AXIS] [get_bd_intf_pins axi_dma_0/S_AXIS_S2MM]
connect_bd_net [get_bd_pins axi_dma_0/s_axi_lite_aclk] [get_bd_pins ps_0/FCLK_CLK0]
connect_bd_net [get_bd_pins axi_dma_0/m_axi_s2mm_aclk] [get_bd_pins ps_0/FCLK_CLK0]
connect_bd_net [get_bd_pins axi_dma_0/axi_resetn] [get_bd_pins proc_sys_reset_0/peripheral_aresetn]
connect_bd_net [get_bd_pins ilconcat_0/In2] [get_bd_pins axi_dma_0/s2mm_introut]

create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 smartconnect_1
set_property CONFIG.NUM_SI {1} [get_bd_cells smartconnect_1]
connect_bd_intf_net [get_bd_intf_pins axi_dma_0/M_AXI_S2MM] [get_bd_intf_pins smartconnect_1/S00_AXI]
connect_bd_intf_net [get_bd_intf_pins smartconnect_1/M00_AXI] [get_bd_intf_pins ps_0/S_AXI_HP0]
connect_bd_net [get_bd_pins smartconnect_1/aclk] [get_bd_pins ps_0/FCLK_CLK0]
connect_bd_net [get_bd_pins ps_0/S_AXI_HP0_ACLK] [get_bd_pins ps_0/FCLK_CLK0]
connect_bd_net [get_bd_pins smartconnect_1/aresetn] [get_bd_pins proc_sys_reset_0/peripheral_aresetn]

assign_bd_address -target_address_space /axi_dma_0/Data_S2MM [get_bd_addr_segs ps_0/S_AXI_HP0/HP0_DDR_LOWOCM] -force
set_property range [get_memory_range ram] [get_bd_addr_segs {axi_dma_0/Data_S2MM/SEG_ps_0_HP0_DDR_LOWOCM}]
set_property offset [get_memory_offset ram] [get_bd_addr_segs {axi_dma_0/Data_S2MM/SEG_ps_0_HP0_DDR_LOWOCM}]

assign_bd_address -target_address_space /ps_0/Data [get_bd_addr_segs axi_dma_0/S_AXI_LITE/Reg] -force
set_property range [get_memory_range dma] [get_bd_addr_segs {ps_0/Data/SEG_axi_dma_0_Reg}]
set_property offset [get_memory_offset dma] [get_bd_addr_segs {ps_0/Data/SEG_axi_dma_0_Reg}]

assign_bd_address -target_address_space /ps_0/Data [get_bd_addr_segs axis_stream_packet_m_0/s_axi/reg0] -force
set_property range [get_memory_range mux] [get_bd_addr_segs {ps_0/Data/SEG_axis_stream_packet_m_0_reg0}]
set_property offset [get_memory_offset mux] [get_bd_addr_segs {ps_0/Data/SEG_axis_stream_packet_m_0_reg0}]