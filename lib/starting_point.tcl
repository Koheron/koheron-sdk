set ps_name ps_0
set rst_name proc_sys_reset_0
set interconnect_0_name axi_mem_intercon_0
set interconnect_1_name axi_mem_intercon_1
set ps_clk $ps_name/FCLK_CLK0

# Create processing_system7
cell xilinx.com:ip:processing_system7:5.5 $ps_name {
  PCW_USE_S_AXI_HP0 0
  PCW_USE_M_AXI_GP1 1
} {
  M_AXI_GP0_ACLK $ps_clk
  M_AXI_GP1_ACLK $ps_clk
}

source $board_preset

# Create all required interconnections
apply_bd_automation -rule xilinx.com:bd_rule:processing_system7 -config {
  make_external {FIXED_IO, DDR}
  Master Disable
  Slave Disable
} [get_bd_cells $ps_name]

# Add processor system reset
cell xilinx.com:ip:proc_sys_reset:5.0 $rst_name {} {
  slowest_sync_clk $ps_clk
  ext_reset_in $ps_name/FCLK_RESET0_N
}

for {set i 0} {$i < 2} {incr i} {
  # Add AXI interconnect
  cell xilinx.com:ip:axi_interconnect:2.1 [set interconnect_${i}_name] {
    S00_HAS_REGSLICE 1
    NUM_MI 1
  } {
    ARESETN $rst_name/interconnect_aresetn
    S00_ARESETN $rst_name/peripheral_aresetn
    M00_ARESETN $rst_name/peripheral_aresetn
    ACLK $ps_clk
    S00_ACLK $ps_clk
    M00_ACLK $ps_clk
  }
  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins [set interconnect_${i}_name]/S00_AXI] [get_bd_intf_pins ps_0/M_AXI_GP$i]
}
