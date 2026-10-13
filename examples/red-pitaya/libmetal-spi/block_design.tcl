set board_preset $board_path/config/board_preset.tcl
source $sdk_path/fpga/lib/starting_point.tcl

set_property CONFIG.PCW_USE_FABRIC_INTERRUPT 1 [get_bd_cells $ps_name]
set_property CONFIG.PCW_IRQ_F2P_INTR 1 [get_bd_cells $ps_name]

# Standard SPI, 8-bit words, AXI-Lite registers, internal loopback enabled in C++.
# Disable STARTUP so the controller cannot drive the board's configuration pins.
cell xilinx.com:ip:axi_quad_spi:3.2 axi_quad_spi_0 {
  C_SPI_MODE 0
  C_NUM_TRANSFER_BITS 8
  C_NUM_SS_BITS 1
  C_FIFO_DEPTH [get_parameter spi_fifo_depth]
  C_SCK_RATIO [get_parameter spi_clock_ratio]
  C_USE_STARTUP 0
  C_TYPE_OF_AXI4_INTERFACE 0
} {
  s_axi_aclk ps_0/FCLK_CLK0
  ext_spi_clk ps_0/FCLK_CLK0
  s_axi_aresetn proc_sys_reset_0/peripheral_aresetn
}

connect_bd_intf_net [get_bd_intf_pins axi_mem_intercon_0/M00_AXI] [get_bd_intf_pins axi_quad_spi_0/AXI_LITE]
connect_pins axi_mem_intercon_0/M00_ACLK ps_0/FCLK_CLK0
connect_pins axi_mem_intercon_0/M00_ARESETN proc_sys_reset_0/peripheral_aresetn
connect_pins axi_quad_spi_0/ip2intc_irpt ps_0/IRQ_F2P

# No SPI ports leave the FPGA; the IP's loopback mux supplies received data.
connect_pins [get_constant_pin 0 1] axi_quad_spi_0/io0_i
connect_pins [get_constant_pin 0 1] axi_quad_spi_0/io1_i
connect_pins [get_constant_pin 0 1] axi_quad_spi_0/sck_i
connect_pins [get_constant_pin 1 1] axi_quad_spi_0/ss_i

assign_bd_address [get_bd_addr_segs axi_quad_spi_0/AXI_LITE/Reg]
set segment [get_bd_addr_segs ps_0/Data/SEG_axi_quad_spi_0_Reg]
set_property offset [get_parameter spi_addr] $segment
set_property range [get_parameter spi_range] $segment
validate_bd_design
