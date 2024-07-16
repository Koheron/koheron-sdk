set ps_name ps_0

# Find the number of interconnects
variable isZynqMP 1
set i 0
while {[info exists config::fclk$i] == 1} {
  set interconnect_${i}_name axi_mem_intercon_$i
  set ps_clk$i $ps_name/pl_clk$i
  incr i
}
set n_interconnects $i

# Create processing_system7
cell xilinx.com:ip:zynq_ultra_ps_e:3.5 $ps_name {
  PSU__USE__M_AXI_GP2 0
  PSU__FPGA_PL1_ENABLE 1
} {}

source $board_preset
set bus_type fpd
set bus_id 0
for {set i 0} {$i < $n_interconnects} {incr i} {
    set_property -dict [list CONFIG.PSU__USE__M_AXI_GP${i} 1] [get_bd_cells $ps_name]
    set_property -dict [list CONFIG.PSU__FPGA_PL${i}_ENABLE {1} \
         CONFIG.PSU__CRL_APB__PL${i}_REF_CTRL__FREQMHZ \
         [expr [set config::fclk$i] / 1000000]] [get_bd_cells $ps_name]
    connect_pins $ps_name/maxihpm${i}_${bus_type}_aclk [set ps_clk$i]
    #incr bus_id
    # if {$i == 1} {
    #   set bus_id 0
    #   set bus_type lpd
    # }

}

set bus_type FPD
set bus_id 0
for {set i 0} {$i < $n_interconnects} {incr i} {
  # Add processor system reset
  set rst${i}_name proc_sys_reset_$i

  cell xilinx.com:ip:proc_sys_reset:5.0 [set rst${i}_name] {} {
    slowest_sync_clk [set ps_clk$i]
    ext_reset_in $ps_name/pl_resetn0
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
  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins [set interconnect_${i}_name]/S00_AXI] [get_bd_intf_pins $ps_name/M_AXI_HPM${i}_${bus_type}]
    # if {$i == 1} {
    #   set bus_id 0
    #   set bus_type LPD
    # }
}
