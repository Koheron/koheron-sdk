# Add PS and AXI Interconnect
set board_preset $board_path/config/board_preset.tcl
source $sdk_path/fpga/lib/starting_point.tcl

source $board_path/adc_dac.tcl

# Add config and status registers
source $sdk_path/fpga/lib/ctl_sts.tcl
add_ctl_sts adc_dac/adc_clk rst_adc_clk/peripheral_aresetn

connect_cell adc_dac {
    ctl [ctl_pin mmcm]
    cfg_data [ps_ctl_pin spi_cfg_data]
    cfg_cmd [ps_ctl_pin spi_cfg_cmd]
    cfg_sts [ps_sts_pin spi_cfg_sts]
}

# Add XADC for monitoring of Zynq temperature

create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_analog_io_rtl:1.0 Vp_Vn

cell xilinx.com:ip:xadc_wiz:3.3 xadc_wiz_0 {
} {
  Vp_Vn Vp_Vn
  s_axi_lite axi_mem_intercon_0/M[add_master_interface 0]_AXI
  s_axi_aclk ps_0/FCLK_CLK0
  s_axi_aresetn proc_sys_reset_0/peripheral_aresetn
}
assign_bd_address [get_bd_addr_segs xadc_wiz_0/s_axi_lite/Reg]

# Expansion connector IOs

for {set i 0} {$i < 8} {incr i} {
  create_bd_port -dir I exp_io_${i}_p
  create_bd_port -dir O exp_io_${i}_n
}

# SPI
source $board_path/spi.tcl

connect_pins ps_0/SDIO0_CDN [get_constant_pin 0 1]
connect_pins ps_0/SDIO0_WP [get_constant_pin 0 1]

# Loopback

#connect_pins adc_dac/adc0 adc_dac/dac0
#connect_pins adc_dac/adc1 adc_dac/dac1

# Add counters

cell xilinx.com:ip:c_counter_binary:12.0 counter_fclk0 {
  Output_Width 64
} {
  CLK ps_0/FCLK_CLK0 
}
connect_pins ps_sts/counter_fclk00 [get_slice_pin counter_fclk0/Q 31 0]
connect_pins ps_sts/counter_fclk01 [get_slice_pin counter_fclk0/Q 63 32]

cell xilinx.com:ip:c_counter_binary:12.0 counter_adc_clk {
  Output_Width 64
} {
  CLK adc_dac/adc_clk
}
connect_pins ps_sts/counter_adc_clk0 [get_slice_pin counter_adc_clk/Q 31 0]
connect_pins ps_sts/counter_adc_clk1 [get_slice_pin counter_adc_clk/Q 63 32]