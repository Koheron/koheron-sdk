source $project_path/ports.tcl

# Add PS and AXI Interconnect
set board_preset $board_path/config/board_preset.tcl
source $sdk_path/fpga/lib/starting_point.tcl

# Add ADCs and DACs
source $project_path/redp_adc_dac_ext_clk.tcl
set adc_dac_name adc_dac
add_redp_adc_dac $adc_dac_name

connect_bd_intf_net [get_bd_intf_ports crystal_clk] -boundary_type upper [get_bd_intf_pins adc_dac/crystal_clk]
connect_bd_intf_net [get_bd_intf_ports sata_clk_in] -boundary_type upper [get_bd_intf_pins adc_dac/sata_clk]

# Add processor system reset synchronous to adc clock
set adc_clk $adc_dac_name/adc_clk
set rst_adc_clk_name proc_sys_reset_adc_clk

cell xilinx.com:ip:proc_sys_reset:5.0 $rst_adc_clk_name {} {
  ext_reset_in $ps_name/FCLK_RESET0_N
  slowest_sync_clk $adc_clk
}

# Clk cfg
set intercon_idx 0
set idx [add_master_interface $intercon_idx]

cell pavel-demin:user:axi_ctl_register:1.0 ctl_clk {
  CTL_DATA_WIDTH 32
} {
  aclk [set ::ps_clk$intercon_idx]
  aresetn [set ::rst${intercon_idx}_name]/peripheral_aresetn
  S_AXI axi_mem_intercon_${intercon_idx}/M${idx}_AXI
}

connect_pins ctl_clk/ctl_data adc_dac/ctl

assign_bd_address [get_bd_addr_segs ctl_clk/s_axi/reg0]
set memory_segment [get_bd_addr_segs ps_0/Data/SEG_ctl_clk_reg0]
set_property range  [get_memory_range ctl_clk]  $memory_segment
set_property offset [get_memory_offset ctl_clk] $memory_segment

connect_pins adc_dac/psclk ps_0/FCLK_CLK0


# Add config and status registers
source $sdk_path/fpga/lib/ctl_sts.tcl
add_ctl_sts $adc_clk $rst_adc_clk_name/peripheral_aresetn

# Connect LEDs
connect_port_pin led_o [get_slice_pin [ctl_pin led] 7 0]

# Connect DAC to config and ADC to status
for {set i 0} {$i < [get_parameter n_adc]} {incr i} {
  connect_pins [sts_pin adc$i] adc_dac/adc[expr $i+1]
}


# SATA
source $project_path/sata.tcl
add_sata sata

connect_ports sata
connect_pins sata/clk $adc_clk
connect_pins sata/dout [sts_pin sts_sata]
connect_pins sata/ctl [ctl_pin ctl_sata]
connect_port_pin dio2p sata/dout
connect_bd_intf_net [get_bd_intf_ports sata_clk_out] -boundary_type upper [get_bd_intf_pins sata/sata_clk_out]

# Add pulse_generator core
cell koheron:user:pulse_generator:1.0 pulse_generator {
  PULSE_WIDTH_WIDTH 32
  PULSE_PERIOD_WIDTH 32
} {
  clk          $adc_clk
  pulse_width  [ctl_pin pulse_width]
  pulse_period [ctl_pin pulse_period]
  rst          [get_slice_pin [ctl_pin trigger] 0 0]
  valid        sata/din
}

# DDS

cell xilinx.com:ip:dds_compiler:6.0 dds {
  PartsPresent Phase_Generator_and_SIN_COS_LUT
  DDS_Clock_Rate 125
  Parameter_Entry Hardware_Parameters
  Phase_Width 48
  Output_Width 14
  Phase_Increment Programmable
} {
  aclk $adc_clk
  m_axis_data_tdata adc_dac/dac1
}

cell pavel-demin:user:axis_variable:1.0 phase_increment {
  AXIS_TDATA_WIDTH 48
} {
  cfg_data [get_concat_pin [list [ctl_pin phase_incr0] [get_slice_pin [ctl_pin phase_incr1] 15 0]]]
  aclk $adc_clk
  aresetn $rst_adc_clk_name/peripheral_aresetn
  M_AXIS dds/S_AXIS_CONFIG
}
