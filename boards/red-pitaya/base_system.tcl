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


# Add dual-port BRAM
source scripts/bram.tcl
add_bram adc_bram_1 8K
add_bram adc_bram_2 8K
add_bram dac_bram   8K

# Create axis_red_pitaya_adc
cell pavel-demin:user:axis_red_pitaya_adc:1.0 adc_0 {} {
  adc_clk_p adc_clk_p_i
  adc_clk_n adc_clk_n_i
  adc_dat_a adc_dat_a_i
  adc_dat_b adc_dat_b_i
  adc_csn adc_csn_o
}

# Create clk_wiz
cell xilinx.com:ip:clk_wiz:5.2 pll_0 {
  PRIMITIVE PLL
  PRIM_IN_FREQ.VALUE_SRC USER
  PRIM_IN_FREQ 125.0
  CLKOUT1_USED true
  CLKOUT1_REQUESTED_OUT_FREQ 250
} {
  clk_in1 adc_0/adc_clk
}

# Create axis_red_pitaya_dac
cell pavel-demin:user:axis_red_pitaya_dac:1.0 dac_0 {} {
  aclk adc_0/adc_clk
  ddr_clk pll_0/clk_out1
  locked pll_0/locked
  S_AXIS adc_0/M_AXIS
  dac_clk dac_clk_o
  dac_rst dac_rst_o
  dac_sel dac_sel_o
  dac_wrt dac_wrt_o
  dac_dat dac_dat_o
}

# Add PWM

cell pavel-demin:user:pwm:1.0 pwm {
  PERIOD 1024
} {
  clk pll_0/clk_out1
}


