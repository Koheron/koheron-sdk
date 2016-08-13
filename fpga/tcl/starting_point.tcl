set ps_name ps_0

# Find the number of interconnects
set i 0
while {[info exists config::fclk$i] == 1} {
  set interconnect_${i}_name axi_mem_intercon_$i
  set ps_clk$i $ps_name/FCLK_CLK$i
  incr i
}
set n_interconnects $i

# Create processing_system7
cell xilinx.com:ip:processing_system7:5.5 $ps_name {
  PCW_USE_S_AXI_HP0 0
  PCW_EN_CLK1_PORT 1
} {}

source $board_preset

for {set i 0} {$i < $n_interconnects} {incr i} {
    set_property -dict [list CONFIG.PCW_USE_M_AXI_GP${i} 1] [get_bd_cells $ps_name]
    set_property -dict [list CONFIG.PCW_FPGA${i}_PERIPHERAL_FREQMHZ [expr [set config::fclk$i] / 1000000]] [get_bd_cells $ps_name]
    connect_pins $ps_name/M_AXI_GP${i}_ACLK [set ps_clk$i]
}

# Create all required interconnections
apply_bd_automation -rule xilinx.com:bd_rule:processing_system7 -config {
  make_external {FIXED_IO, DDR}
  Master Disable
  Slave Disable
} [get_bd_cells $ps_name]

for {set i 0} {$i < $n_interconnects} {incr i} {
  # Add processor system reset
  set rst${i}_name proc_sys_reset_$i

  cell xilinx.com:ip:proc_sys_reset:5.0 [set rst${i}_name] {} {
    slowest_sync_clk [set ps_clk$i]
    ext_reset_in $ps_name/FCLK_RESET0_N
  }
  # Add AXI interconnect
  cell xilinx.com:ip:axi_interconnect:2.1 [set interconnect_${i}_name] {
    NUM_MI 1
  } {
    ARESETN [set rst${i}_name]/interconnect_aresetn
    S00_ARESETN [set rst${i}_name]/peripheral_aresetn
    ACLK [set ps_clk$i]
    S00_ACLK [set ps_clk$i]
  }
  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins [set interconnect_${i}_name]/S00_AXI] [get_bd_intf_pins $ps_name/M_AXI_GP$i]
}
