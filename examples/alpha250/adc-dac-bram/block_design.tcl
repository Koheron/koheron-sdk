# Add PS and AXI Interconnect
set board_preset $board_path/config/board_preset.tcl
source $sdk_path/fpga/lib/starting_point.tcl

source $board_path/adc_dac.tcl

# Add config and status registers
source $sdk_path/fpga/lib/ctl_sts.tcl
add_ctl_sts adc_dac/adc_clk rst_adc_clk/peripheral_aresetn

connect_cell adc_dac {
    ctl [ctl_pin mmcm]
    psclk adc_dac/adc_clk
    cfg_data [ctl_pin spi_cfg_data]
    cfg_cmd [ctl_pin spi_cfg_cmd]
    cfg_sts [sts_pin spi_cfg_sts]
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

# Add BRAMs for ADC and DAC
# "adc" and "dac" BRAM ranges and offsets are defined in the "memory" part of config.yml
source $sdk_path/fpga/lib/bram.tcl
add_bram adc
add_bram dac

# Add a counter for BRAM addressing
cell koheron:user:address_counter:1.0 address_counter {
  COUNT_WIDTH 15
} {
  clken [get_constant_pin 1 1]
  clk adc_dac/adc_clk
  trig [get_slice_pin [ctl_pin trig] 0 0]
}

connect_cell blk_mem_gen_adc {
  addrb address_counter/address
  clkb adc_dac/adc_clk
  dinb [get_Q_pin [get_concat_pin [list adc_dac/adc0 adc_dac/adc1]] 1 noce adc_dac/adc_clk]
  enb [get_constant_pin 1 1]
  rstb [get_constant_pin 0 1]
  web address_counter/wen
}

connect_cell blk_mem_gen_dac {
  addrb address_counter/address
  clkb adc_dac/adc_clk
  enb [get_constant_pin 1 1]
  rstb [get_constant_pin 0 1]
  web [get_constant_pin 0 4]
}

connect_pins adc_dac/dac0 [get_Q_pin [get_slice_pin blk_mem_gen_dac/doutb 15 0] 1 noce adc_dac/adc_clk]
connect_pins adc_dac/dac1 [get_Q_pin [get_slice_pin blk_mem_gen_dac/doutb 31 16] 1 noce adc_dac/adc_clk]