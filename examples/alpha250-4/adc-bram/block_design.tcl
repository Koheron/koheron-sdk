# Add PS and AXI Interconnect
set board_preset $board_path/config/board_preset.tcl
source $sdk_path/fpga/lib/starting_point.tcl

set adc_dac_extra_delay 2
source $board_path/adc.tcl

# Add config and status registers
source $sdk_path/fpga/lib/ctl_sts.tcl
add_ctl_sts adc/adc_clk rst_adc_clk/peripheral_aresetn

connect_cell adc {
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

# Add BRAMs for ADC and DAC
# "adc" and "dac" BRAM ranges and offsets are defined in the "memory" part of config.yml
source $sdk_path/fpga/lib/bram.tcl

# Add a counter for BRAM addressing
cell koheron:user:address_counter:1.0 address_counter {
  COUNT_WIDTH [get_memory_addr_width adc0]
} {
  clken [get_constant_pin 1 1]
  clk adc/adc_clk
  trig [get_slice_pin [ctl_pin trig] 0 0]
}

for {set j 0} {$j < 2} {incr j} { # ADC index
  add_bram adc$j

  connect_cell blk_mem_gen_adc$j {
    addrb address_counter/address
    clkb adc/adc_clk
    dinb [get_concat_pin [list adc/adc${j}0 adc/adc${j}1]]
    enb [get_constant_pin 1 1]
    rstb [get_constant_pin 0 1]
    web address_counter/wen
  }
}