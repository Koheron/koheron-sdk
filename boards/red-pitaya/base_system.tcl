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
#source scripts/bram.tcl
#add_bram adc_bram_1 8K
#add_bram adc_bram_2 8K
#add_bram dac_bram   8K

# Phase-locked Loop (PLL)
cell xilinx.com:ip:clk_wiz:5.2 pll {
  PRIMITIVE              PLL
  PRIM_IN_FREQ.VALUE_SRC USER
  PRIM_IN_FREQ           125.0
  PRIM_SOURCE            Differential_clock_capable_pin
  CLKOUT1_USED true CLKOUT1_REQUESTED_OUT_FREQ 125.0
  CLKOUT2_USED true CLKOUT2_REQUESTED_OUT_FREQ 125.0
  CLKOUT3_USED true CLKOUT3_REQUESTED_OUT_FREQ 250.0
  CLKOUT4_USED true CLKOUT4_REQUESTED_OUT_FREQ 250.0 CLKOUT4_REQUESTED_PHASE -45
  CLKOUT5_USED true CLKOUT5_REQUESTED_OUT_FREQ 250.0
  CLKOUT6_USED true CLKOUT6_REQUESTED_OUT_FREQ 250.0
  RESET_TYPE ACTIVE_LOW RESET_PORT resetn
} {}
connect_bd_net [get_bd_ports adc_clk_p_i] [get_bd_pins pll/clk_in1_p]
connect_bd_net [get_bd_ports adc_clk_n_i] [get_bd_pins pll/clk_in1_n]

# Add ADC IP block
set adc_name adc_0
create_bd_cell -type ip -vlnv pavel-demin:user:redp_adc:1.0 $adc_name
foreach {port_name} {
  adc_dat_a_i
  adc_dat_b_i
  adc_clk_source
  adc_cdcs_o
} {
  connect_bd_net [get_bd_ports $port_name] [get_bd_pins $adc_name/$port_name]
}
connect_bd_net [get_bd_pins $adc_name/adc_clk]    [get_bd_pins pll/clk_out1]

# Add DAC IP block
set dac_name dac_0
create_bd_cell -type ip -vlnv pavel-demin:user:redp_dac:1.0 $dac_name
foreach {port_name} {
  dac_clk_o
  dac_dat_o
  dac_rst_o
  dac_sel_o
  dac_wrt_o
} {
  connect_bd_net [get_bd_ports $port_name] [get_bd_pins $dac_name/$port_name]
}
connect_bd_net [get_bd_pins $dac_name/dac_clk_1x] [get_bd_pins pll/clk_out2]
connect_bd_net [get_bd_pins $dac_name/dac_clk_2x] [get_bd_pins pll/clk_out3]
connect_bd_net [get_bd_pins $dac_name/dac_clk_2p] [get_bd_pins pll/clk_out4]
connect_bd_net [get_bd_pins $dac_name/dac_locked] [get_bd_pins pll/locked]

# Connect reset
create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 adc_rst
connect_bd_net [get_bd_pins adc_rst/dout] [get_bd_pins $adc_name/adc_rst_i]

# Add AXI configuration register (synchronous with ADC clock)

set_property -dict [list CONFIG.NUM_MI {3}] [get_bd_cells ps_0_axi_periph]
connect_bd_net [get_bd_pins /ps_0_axi_periph/M02_ACLK] [get_bd_pins ps_0/FCLK_CLK0]
connect_bd_net [get_bd_pins ps_0_axi_periph/M02_ARESETN] [get_bd_pins rst_ps_0_125M/peripheral_aresetn]
# Add AXI clock converter
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_clock_converter:2.1 axi_clock_converter_0
connect_bd_intf_net -boundary_type upper [get_bd_intf_pins ps_0_axi_periph/M02_AXI] [get_bd_intf_pins axi_clock_converter_0/S_AXI]
connect_bd_net [get_bd_pins axi_clock_converter_0/s_axi_aclk] [get_bd_pins ps_0/FCLK_CLK0]
connect_bd_net [get_bd_pins axi_clock_converter_0/s_axi_aresetn] [get_bd_pins rst_ps_0_125M/peripheral_aresetn]
connect_bd_net [get_bd_pins axi_clock_converter_0/m_axi_aclk] [get_bd_pins pll/clk_out1]
connect_bd_net [get_bd_pins axi_clock_converter_0/m_axi_aresetn] [get_bd_pins rst_ps_0_125M/peripheral_aresetn]

create_bd_cell -type ip -vlnv pavel-demin:user:axi_cfg_register:1.0 axi_cfg_register_0
connect_bd_intf_net [get_bd_intf_pins axi_cfg_register_0/S_AXI] [get_bd_intf_pins axi_clock_converter_0/M_AXI]
connect_bd_net [get_bd_pins axi_cfg_register_0/aclk] [get_bd_pins axi_clock_converter_0/m_axi_aclk]
connect_bd_net [get_bd_pins axi_cfg_register_0/aresetn] [get_bd_pins rst_ps_0_125M/peripheral_aresetn]
assign_bd_address [get_bd_addr_segs {axi_cfg_register_0/s_axi/reg0 }]
set_property range 4K [get_bd_addr_segs {ps_0/Data/SEG_axi_cfg_register_0_reg0}]


# Connect LEDs
set led_offset 0
cell xilinx.com:ip:xlslice:1.0 led_slice \
  [list DIN_WIDTH 1024 DIN_FROM [expr 7+$led_offset] DIN_TO [expr $led_offset]] \
  [list Din axi_cfg_register_0/cfg_data]
connect_bd_net [get_bd_ports led_o] [get_bd_pins led_slice/Dout]

# Add PWM
set n_pwm 4
set pwm_offset 32
cell xilinx.com:ip:xlconcat:2.1 concat_pwm [list NUM_PORTS $n_pwm] {}
connect_bd_net [get_bd_ports dac_pwm_o] [get_bd_pins concat_pwm/dout]

for {set i 0} {$i < $n_pwm} {incr i} {
  cell pavel-demin:user:pwm:1.0 pwm_$i {NBITS 10} \
    [list clk pll/clk_out6 rst rst_ps_0_125M/peripheral_reset pwm_out concat_pwm/In$i]
  cell xilinx.com:ip:xlslice:1.0 pwm_slice_$i \
    [list DIN_WIDTH 1024 DIN_FROM [expr 9+32*$i+$pwm_offset] DIN_TO [expr 32*$i+$pwm_offset]] \
    [list Din axi_cfg_register_0/cfg_data Dout pwm_$i/threshold]
}
