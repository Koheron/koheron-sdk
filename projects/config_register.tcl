
#proc add_config_register {} 

set_property -dict [list CONFIG.NUM_MI {3}] [get_bd_cells ps_0_axi_periph]
connect_bd_net [get_bd_pins /ps_0_axi_periph/M02_ACLK] [get_bd_pins ps_0/FCLK_CLK0]
connect_bd_net [get_bd_pins ps_0_axi_periph/M02_ARESETN] [get_bd_pins rst_ps_0_125M/peripheral_aresetn]
# AXI clock converter
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_clock_converter:2.1 axi_clock_converter_0
connect_bd_intf_net -boundary_type upper [get_bd_intf_pins ps_0_axi_periph/M02_AXI] [get_bd_intf_pins axi_clock_converter_0/S_AXI]
connect_bd_net [get_bd_pins axi_clock_converter_0/s_axi_aclk] [get_bd_pins ps_0/FCLK_CLK0]
connect_bd_net [get_bd_pins axi_clock_converter_0/s_axi_aresetn] [get_bd_pins rst_ps_0_125M/peripheral_aresetn]
connect_bd_net [get_bd_pins axi_clock_converter_0/m_axi_aclk] [get_bd_pins $adc_clk]
connect_bd_net [get_bd_pins axi_clock_converter_0/m_axi_aresetn] [get_bd_pins rst_ps_0_125M/peripheral_aresetn]
# Cfg register
create_bd_cell -type ip -vlnv pavel-demin:user:axi_cfg_register:1.0 axi_cfg_register_0
connect_bd_intf_net [get_bd_intf_pins axi_cfg_register_0/S_AXI] [get_bd_intf_pins axi_clock_converter_0/M_AXI]
connect_bd_net [get_bd_pins axi_cfg_register_0/aclk] [get_bd_pins axi_clock_converter_0/m_axi_aclk]
connect_bd_net [get_bd_pins axi_cfg_register_0/aresetn] [get_bd_pins rst_ps_0_125M/peripheral_aresetn]
assign_bd_address [get_bd_addr_segs {axi_cfg_register_0/s_axi/reg0 }]
set_property range 4K [get_bd_addr_segs {ps_0/Data/SEG_axi_cfg_register_0_reg0}]

