set board_preset boards/$board_name/config/board_preset.xml

# Create processing_system7
set ps_name ps_0
cell xilinx.com:ip:processing_system7:5.5 $ps_name [list PCW_IMPORT_BOARD_PRESET $board_preset PCW_USE_S_AXI_HP0 0] [list M_AXI_GP0_ACLK ps_0/FCLK_CLK0]

# Create all required interconnections
apply_bd_automation -rule xilinx.com:bd_rule:processing_system7 -config {
  make_external {FIXED_IO, DDR}
  Master Disable
  Slave Disable
} [get_bd_cells $ps_name]

# Add XADC Wizard and AXI Interconnect
set xadc_name xadc_wiz_0
create_bd_cell -type ip -vlnv xilinx.com:ip:xadc_wiz:3.2 $xadc_name

apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config [list Master "/${ps_name}/M_AXI_GP0" Clk "Auto"] [get_bd_intf_pins $xadc_name/s_axi_lite]

properties $xadc_name {
  XADC_STARUP_SELECTION independent_adc
  OT_ALARM false
  USER_TEMP_ALARM false
  VCCINT_ALARM false
  VCCAUX_ALARM false
  ENABLE_VCCPINT_ALARM false
  ENABLE_VCCPAUX_ALARM false
  ENABLE_VCCDDRO_ALARM false
  CHANNEL_ENABLE_VAUXP0_VAUXN0 true
  CHANNEL_ENABLE_VAUXP1_VAUXN1 true
  CHANNEL_ENABLE_VAUXP8_VAUXN8 true
  CHANNEL_ENABLE_VAUXP9_VAUXN9 true
  CHANNEL_ENABLE_VP_VN true
}

foreach {port_name} {
  Vp_Vn
  Vaux0
  Vaux1
  Vaux8
  Vaux9
} {
  connect_bd_intf_net [get_bd_intf_pins $xadc_name/$port_name] [get_bd_intf_ports $port_name]
}

# Add GPIO
# Add a new Master Interface to AXI Interconnect
set num_master_interfaces [get_property CONFIG.NUM_MI [get_bd_cells ${ps_name}_axi_periph]]
incr num_master_interfaces
properties ${ps_name}_axi_periph [list NUM_MI $num_master_interfaces]

set gpio_name axi_gpio_0
cell xilinx.com:ip:axi_gpio:2.0 $gpio_name {
  C_GPIO_WIDTH 8
  C_GPIO2_WIDTH 8
  C_IS_DUAL 1
} {}

apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config [list Master "/${ps_name}/M_AXI_GP0" Clk "Auto"]  [get_bd_intf_pins $gpio_name/S_AXI]
apply_bd_automation -rule xilinx.com:bd_rule:board  [get_bd_intf_pins $gpio_name/GPIO]
apply_bd_automation -rule xilinx.com:bd_rule:board  [get_bd_intf_pins $gpio_name/GPIO2]
set_property name exp_n [get_bd_intf_ports gpio_rtl]
set_property name exp_p [get_bd_intf_ports gpio_rtl_0]

# Add ADCs and DACs
source boards/red-pitaya/adc_dac.tcl
# Rename clocks
set adc_clk adc_dac/pll/clk_out1
set pwm_clk adc_dac/pll/clk_out6

# Add Configuration register (synchronous with ADC clock)
source projects/config_register.tcl

# Add Status register
# TODO

# Connect LEDs
set led_offset 0
cell xilinx.com:ip:xlslice:1.0 led_slice \
  [list DIN_WIDTH 1024 DIN_FROM [expr 7+$led_offset] DIN_TO [expr $led_offset]] \
  [list Din axi_cfg_register_0/cfg_data]
connect_bd_net [get_bd_ports led_o] [get_bd_pins led_slice/Dout]

# Add PWM
source boards/red-pitaya/pwm.tcl

# Add address counter
set bram_width 13
cell xilinx.com:ip:c_counter_binary:12.0 base_counter \
  [list Output_Width [expr $bram_width+2] Increment_Value 4] \
  [list CLK $adc_clk]

# Add DAC BRAM
source scripts/bram.tcl
set dac_bram_name dac_bram
add_bram $dac_bram_name 32K
# Connect port B of BRAM to ADC clock
connect_bd_net [get_bd_pins blk_mem_gen_$dac_bram_name/clkb] [get_bd_pins $adc_clk]
connect_bd_net [get_bd_pins blk_mem_gen_$dac_bram_name/addrb] [get_bd_pins base_counter/Q]

# Connect BRAM output to DACs
for {set i 0} {$i < 2} {incr i} {
  set channel [lindex {a b} $i]
  cell xilinx.com:ip:xlslice:1.0 dac_${channel}_slice \
    [list DIN_WIDTH 32 DIN_FROM [expr 13+16*$i] DIN_TO [expr 16*$i]] \
    [list Din blk_mem_gen_$dac_bram_name/doutb Dout adc_dac/$dac_name/dac_dat_${channel}_i]
}

# Connect remaining ports of BRAM
cell xilinx.com:ip:xlconstant:1.1 ${dac_bram_name}_dinb {CONST_VAL 0 CONST_WIDTH 32} [list dout blk_mem_gen_$dac_bram_name/dinb]
cell xilinx.com:ip:xlconstant:1.1 ${dac_bram_name}_enb {CONST_VAL 1} [list dout blk_mem_gen_$dac_bram_name/enb]
cell xilinx.com:ip:xlconstant:1.1 ${dac_bram_name}_web {CONST_VAL 0 CONST_WIDTH 4} [list dout blk_mem_gen_$dac_bram_name/web]
connect_bd_net [get_bd_pins blk_mem_gen_$dac_bram_name/rstb] [get_bd_pins rst_ps_0_125M/peripheral_reset]

# Add ADC1 BRAM
set adc1_bram_name adc1_bram
add_bram $adc1_bram_name 32K
# Connect port B of BRAM to ADC clock
connect_bd_net [get_bd_pins blk_mem_gen_$adc1_bram_name/clkb] [get_bd_pins $adc_clk]
cell xilinx.com:ip:xlconstant:1.1 ${adc1_bram_name}_enb {CONST_VAL 1} [list dout blk_mem_gen_$adc1_bram_name/enb]
connect_bd_net [get_bd_pins blk_mem_gen_$adc1_bram_name/addrb] [get_bd_pins base_counter/Q]
connect_bd_net [get_bd_pins blk_mem_gen_$adc1_bram_name/rstb] [get_bd_pins rst_ps_0_125M/peripheral_reset]

# Add averaging module
source projects/averaging.tcl


