set ps_name xdma_0
variable dclass "xdma"

# Find the number of interconnects
set i 0
while {[info exists config::fclk$i] == 1} {
  set interconnect_${i}_name axi_mem_intercon_$i
  set ps_clk$i $ps_name/axi_aclk
  incr i
}
set n_interconnects $i

create_bd_intf_port -mode Master -vlnv xilinx.com:interface:pcie_7x_mgt_rtl:1.0 pcie_7x_mgt 
create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 PCIe_CLK 
create_bd_port -dir I -type rst PCIe_RESETN

# Create processing_system7
cell xilinx.com:ip:util_ds_buf:2.1 util_ds_buf_0 {
   C_BUF_TYPE IBUFDSGTE
} {
  CLK_IN_D PCIe_CLK
}
cell xilinx.com:ip:xdma:4.1 $ps_name {
   PF0_DEVICE_ID_mqdma {9024} 
   PF2_DEVICE_ID_mqdma {9024} 
   PF3_DEVICE_ID_mqdma {9024} 
   axi_data_width {128_bit} 
   axilite_master_en {true} 
   axilite_master_size {2} 
   axilite_master_scale {Gigabytes} 
   xdma_rnum_chnl {2} 
   xdma_wnum_chnl {2} 
   dsc_bypass_rd  {0011}
   dsc_bypass_wr {0011}
   axist_bypass_en {false} 
   axisten_freq {125} 
   en_transceiver_status_ports {false} 
   pcie_extended_tag {false} 
   cfg_mgmt_if false
   pf0_device_id {7024} 
   pf0_msi_enabled {false} 
   xdma_axi_intf_mm {AXI_Stream}
   pf0_msix_cap_pba_bir {BAR_1} 
   pf0_msix_cap_table_bir {BAR_1} 
   pl_link_cap_max_link_speed {5.0_GT/s} 
   pl_link_cap_max_link_width {X4} 
   plltype {QPLL1} 
} {
  usr_irq_req [get_constant_pin 0 1]
  sys_rst_n PCIe_RESETN
  sys_clk util_ds_buf_0/IBUF_OUT

  pcie_mgt pcie_7x_mgt
}

for {set i 0} {$i < $n_interconnects} {incr i} {
  # Add processor system reset
  set rst${i}_name proc_sys_reset_$i

  cell xilinx.com:ip:proc_sys_reset:5.0 [set rst${i}_name] {} {
    slowest_sync_clk $ps_name/axi_aclk
    ext_reset_in $ps_name/axi_aresetn
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
  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins [set interconnect_${i}_name]/S00_AXI] [get_bd_intf_pins $ps_name/M_AXI_LITE]
    if {$i == 1} {
      set bus_id 0
      set bus_type LPD
    }
}


source $board_preset

