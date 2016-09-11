source boards/$board_name/config/ports.tcl

# Add PS and AXI Interconnect
set board_preset boards/$board_name/config/board_preset.tcl
source $lib/starting_point.tcl

# Add ADCs and DACs
source $lib/redp_adc_dac.tcl
set adc_dac_name adc_dac
add_redp_adc_dac $adc_dac_name

# Rename clocks
set adc_clk $adc_dac_name/adc_clk

# Add processor system reset synchronous to adc clock
set rst_adc_clk_name proc_sys_reset_adc_clk
cell xilinx.com:ip:proc_sys_reset:5.0 $rst_adc_clk_name {} {
  ext_reset_in $ps_name/FCLK_RESET0_N
  slowest_sync_clk $adc_clk
}

# Add config and status registers
source $lib/cfg_sts.tcl
add_cfg_sts $adc_clk $rst_adc_clk_name/peripheral_aresetn

# Connect LEDs
connect_bd_net [get_bd_ports led_o] [get_bd_pins [get_slice_pin [cfg_pin led] 7 0]]

# Connect ADC to status register
for {set i 0} {$i < [get_parameter n_adc]} {incr i} {
  connect_pins [sts_pin adc$i] adc_dac/adc[expr $i + 1]
}

# Add DAC controller
source $lib/bram.tcl
set bram_name dac
add_bram $bram_name [get_address_range $bram_name] [get_address_offset $bram_name] 0

set dac_bram_name blk_mem_gen_${bram_name}

connect_pins adc_dac/dac1 [get_slice_pin $dac_bram_name/doutb 13 0]
connect_pins adc_dac/dac2 [get_slice_pin $dac_bram_name/doutb 29 16]

connect_cell $dac_bram_name {
  web  [get_constant_pin 0 4]
  dinb [get_constant_pin 0 32]
  clkb $adc_clk
  rstb $rst_adc_clk_name/peripheral_reset
}

# Add pulse_generator core
cell koheron:user:pulse_generator:1.0 pulse_generator {
  PULSE_WIDTH_WIDTH $config::bram_addr_width
  PULSE_PERIOD_WIDTH 30
} {
  clk          $adc_clk
  pulse_width  [cfg_pin pulse_width]
  pulse_period [cfg_pin pulse_period]
  valid        $dac_bram_name/enb
  rst          [get_slice_pin [cfg_pin trigger] 0 0]
  cnt          [sts_pin count]
}

# Use (pulse_generator/cnt << 2) to address DAC bram
connect_pins \
  $dac_bram_name/addrb \
  [get_concat_pin [list [get_constant_pin 0 2] pulse_generator/cnt]]

# pulse_data = { start_pulse, 0, adc2[13:0], start_pulse, 0, adc1[13:0] }
set pulse_data [get_concat_pin [list \
  adc_dac/adc1 \
  [get_constant_pin 0 1] \
  pulse_generator/start \
  adc_dac/adc2 \
  [get_constant_pin 0 1] \
  pulse_generator/start]]

# Use AXI Stream clock converter (ADC clock -> FPGA clock)
set intercon_idx 0
set idx [add_master_interface $intercon_idx]
cell xilinx.com:ip:axis_clock_converter:1.1 adc_clock_converter {
  TDATA_NUM_BYTES 4
} {
  s_axis_tdata $pulse_data
  s_axis_tvalid pulse_generator/valid
  s_axis_aresetn $rst_adc_clk_name/peripheral_aresetn
  m_axis_aresetn [set rst${intercon_idx}_name]/peripheral_aresetn
  s_axis_aclk $adc_clk
  m_axis_aclk [set ps_clk$intercon_idx]
}

# Add AXI stream FIFO to read pulse data from the PS
cell xilinx.com:ip:axi_fifo_mm_s:4.1 adc_axis_fifo {
  C_USE_TX_DATA 0
  C_USE_TX_CTRL 0
  C_USE_RX_CUT_THROUGH true
  C_RX_FIFO_DEPTH 16384
  C_RX_FIFO_PF_THRESHOLD 8192
} {
  s_axi_aclk [set ps_clk$intercon_idx]
  s_axi_aresetn [set rst${intercon_idx}_name]/peripheral_aresetn
  S_AXI [set interconnect_${intercon_idx}_name]/M${idx}_AXI
  AXI_STR_RXD adc_clock_converter/M_AXIS
}

assign_bd_address [get_bd_addr_segs adc_axis_fifo/S_AXI/Mem0]
set memory_segment [get_bd_addr_segs /${::ps_name}/Data/SEG_adc_axis_fifo_Mem0]
set_property offset [get_address_offset adc_fifo] $memory_segment
set_property range [get_address_range adc_fifo] $memory_segment

