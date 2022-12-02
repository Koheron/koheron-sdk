
create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_analog_io_rtl:1.0 Vp_Vn

cell xilinx.com:ip:xadc_wiz:3.3 xadc_wiz_0 {
  CHANNEL_ENABLE_VP_VN {false}
} {
  Vp_Vn Vp_Vn
  s_axi_lite axi_mem_intercon_0/M[add_master_interface]_AXI
  s_axi_aclk ${ps_name}/FCLK_CLK0
  s_axi_aresetn proc_sys_reset_0/peripheral_aresetn
}
assign_bd_address [get_bd_addr_segs xadc_wiz_0/s_axi_lite/Reg]


