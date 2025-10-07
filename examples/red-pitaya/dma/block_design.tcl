
set ps_name ps_0
create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7:5.5 $ps_name

source $board_path/config/board_preset.tcl

create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 proc_sys_reset_0

set_property -dict [list \
  CONFIG.PCW_FCLK0_PERIPHERAL_CLKSRC {ARM PLL} \
  CONFIG.PCW_FPGA0_PERIPHERAL_FREQMHZ {133.34} \
  CONFIG.PCW_USE_S_AXI_HP0 {1} \
  CONFIG.PCW_USE_S_AXI_HP2 {1} \
  CONFIG.PCW_USE_HIGH_OCM {1} \
  CONFIG.PCW_USE_S_AXI_GP0 1 \
] [get_bd_cells ps_0]

apply_bd_automation -rule xilinx.com:bd_rule:processing_system7 -config {make_external "FIXED_IO, DDR" Master "Disable" Slave "Disable" }  [get_bd_cells ps_0]

create_bd_cell -type ip -vlnv xilinx.com:ip:axi_dma:7.1 axi_dma_0

set_property -dict [list \
  CONFIG.c_sg_length_width {26} \
  CONFIG.c_include_sg {1} \
  CONFIG.c_m_axi_mm2s_data_width {64} \
  CONFIG.c_m_axis_mm2s_tdata_width {64} \
  CONFIG.c_mm2s_burst_size {64} \
  CONFIG.c_s2mm_burst_size {64} \
  CONFIG.c_sg_include_stscntrl_strm {0} \
] [get_bd_cells axi_dma_0]

connect_bd_intf_net [get_bd_intf_pins axi_dma_0/M_AXIS_MM2S] [get_bd_intf_pins axi_dma_0/S_AXIS_S2MM]

apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} Master {/ps_0/M_AXI_GP0} Slave {/axi_dma_0/S_AXI_LITE} ddr_seg {Auto} intc_ip {New AXI SmartConnect} master_apm {0}}  [get_bd_intf_pins axi_dma_0/S_AXI_LITE]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} Master {/axi_dma_0/M_AXI_MM2S} Slave {/ps_0/S_AXI_GP0} ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0}}  [get_bd_intf_pins ps_0/S_AXI_GP0]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} Master {/axi_dma_0/M_AXI_MM2S} Slave {/ps_0/S_AXI_HP0} ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0}}  [get_bd_intf_pins ps_0/S_AXI_HP0]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {Auto} Clk_xbar {Auto} Master {/axi_dma_0/M_AXI_MM2S} Slave {/ps_0/S_AXI_HP2} ddr_seg {Auto} intc_ip {New AXI Interconnect} master_apm {0}}  [get_bd_intf_pins ps_0/S_AXI_HP2]

apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {/ps_0/FCLK_CLK0 (100 MHz)} Clk_xbar {/ps_0/FCLK_CLK0 (100 MHz)} Master {/axi_dma_0/M_AXI_S2MM} Slave {/ps_0/S_AXI_GP0} ddr_seg {Auto} intc_ip {/axi_mem_intercon} master_apm {0}}  [get_bd_intf_pins axi_dma_0/M_AXI_S2MM]
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config { Clk_master {Auto} Clk_slave {/ps_0/FCLK_CLK0 (100 MHz)} Clk_xbar {/ps_0/FCLK_CLK0 (100 MHz)} Master {/axi_dma_0/M_AXI_SG} Slave {/ps_0/S_AXI_GP0} ddr_seg {Auto} intc_ip {/axi_mem_intercon} master_apm {0}}  [get_bd_intf_pins axi_dma_0/M_AXI_SG]

create_bd_cell -type inline_hdl -vlnv xilinx.com:inline_hdl:ilconcat:1.0 ilconcat_0
connect_bd_net [get_bd_pins axi_dma_0/mm2s_introut] [get_bd_pins ilconcat_0/In0]
connect_bd_net [get_bd_pins axi_dma_0/s2mm_introut] [get_bd_pins ilconcat_0/In1]
connect_bd_net [get_bd_pins ilconcat_0/dout] [get_bd_pins ps_0/IRQ_F2P]

delete_bd_objs [get_bd_addr_segs] [get_bd_addr_segs -excluded]

assign_bd_address -target_address_space /axi_dma_0/Data_S2MM [get_bd_addr_segs ps_0/S_AXI_HP0/HP0_DDR_LOWOCM] -force
set_property range [get_memory_range ram_s2mm] [get_bd_addr_segs {axi_dma_0/Data_S2MM/SEG_ps_0_HP0_DDR_LOWOCM}]
set_property offset [get_memory_offset ram_s2mm] [get_bd_addr_segs {axi_dma_0/Data_S2MM/SEG_ps_0_HP0_DDR_LOWOCM}]

assign_bd_address -target_address_space /axi_dma_0/Data_MM2S [get_bd_addr_segs ps_0/S_AXI_HP2/HP2_DDR_LOWOCM] -force
set_property range [get_memory_range ram_mm2s] [get_bd_addr_segs {axi_dma_0/Data_MM2S/SEG_ps_0_HP2_DDR_LOWOCM}]
set_property offset [get_memory_offset ram_mm2s] [get_bd_addr_segs {axi_dma_0/Data_MM2S/SEG_ps_0_HP2_DDR_LOWOCM}]

assign_bd_address -target_address_space /axi_dma_0/Data_SG [get_bd_addr_segs ps_0/S_AXI_GP0/GP0_HIGH_OCM] -force
set_property range 64K [get_bd_addr_segs {axi_dma_0/Data_SG/SEG_ps_0_GP0_HIGH_OCM}]
set_property offset [get_memory_offset ocm_mm2s] [get_bd_addr_segs {axi_dma_0/Data_SG/SEG_ps_0_GP0_HIGH_OCM}]

assign_bd_address -target_address_space /ps_0/Data [get_bd_addr_segs axi_dma_0/S_AXI_LITE/Reg] -force
set_property range [get_memory_range dma] [get_bd_addr_segs {ps_0/Data/SEG_axi_dma_0_Reg}]
set_property offset [get_memory_offset dma] [get_bd_addr_segs {ps_0/Data/SEG_axi_dma_0_Reg}]

exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_HP2/HP2_HIGH_OCM] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_MM2S]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_GP0/GP0_DDR_LOWOCM] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_MM2S]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_GP0/GP0_HIGH_OCM] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_MM2S]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_GP0/GP0_IOP] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_MM2S]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_GP0/GP0_QSPI_LINEAR] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_MM2S]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_GP0/GP0_M_AXI_GP0] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_MM2S]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_HP0/HP0_HIGH_OCM] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_MM2S]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_HP0/HP0_DDR_LOWOCM] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_MM2S]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_HP2/HP2_DDR_LOWOCM] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_S2MM]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_GP0/GP0_HIGH_OCM] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_S2MM]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_GP0/GP0_QSPI_LINEAR] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_S2MM]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_GP0/GP0_IOP] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_S2MM]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_GP0/GP0_DDR_LOWOCM] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_S2MM]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_GP0/GP0_M_AXI_GP0] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_S2MM]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_HP0/HP0_HIGH_OCM] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_S2MM]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_HP2/HP2_HIGH_OCM] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_S2MM]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_HP2/HP2_HIGH_OCM] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_SG]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_GP0/GP0_DDR_LOWOCM] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_SG]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_GP0/GP0_QSPI_LINEAR] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_SG]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_GP0/GP0_M_AXI_GP0] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_SG]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_GP0/GP0_IOP] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_SG]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_HP0/HP0_HIGH_OCM] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_SG]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_HP0/HP0_DDR_LOWOCM] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_SG]
exclude_bd_addr_seg [get_bd_addr_segs ps_0/S_AXI_HP2/HP2_DDR_LOWOCM] -target_address_space [get_bd_addr_spaces axi_dma_0/Data_SG]