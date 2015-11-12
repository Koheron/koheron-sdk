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

# Add ADC/DAC IP block
set adc_dac_name adc_dac_0
create_bd_cell -type ip -vlnv pavel-demin:user:redp_adc_dac:1.0 $adc_dac_name
foreach {port_name} {
  adc_dat_a_i
  adc_dat_b_i
  adc_clk_p_i
  adc_clk_n_i
  adc_clk_source
  adc_cdcs_o
  dac_clk_o
  dac_dat_o
  dac_rst_o
  dac_sel_o
  dac_wrt_o
} {
  connect_bd_net [get_bd_ports $port_name] [get_bd_pins $adc_dac_name/$port_name]
}
# Connect reset
create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 adc_rst
connect_bd_net [get_bd_pins adc_rst/dout] [get_bd_pins $adc_dac_name/adc_rst_i]

# Add PWM
cell pavel-demin:user:pwm:1.0 pwm {
  PERIOD 1024
} {
  clk adc_dac_0/adc_clk_o
}

# Add AXI configuration register from red-pitaya-notes

set_property -dict [list CONFIG.NUM_MI {3}] [get_bd_cells ps_0_axi_periph]
connect_bd_net [get_bd_pins /ps_0_axi_periph/M02_ACLK] [get_bd_pins ps_0/FCLK_CLK0]
connect_bd_net [get_bd_pins ps_0_axi_periph/M02_ARESETN] [get_bd_pins rst_ps_0_142M/peripheral_aresetn]

create_bd_cell -type ip -vlnv xilinx.com:ip:axi_clock_converter:2.1 axi_clock_converter_0

connect_bd_intf_net -boundary_type upper [get_bd_intf_pins ps_0_axi_periph/M02_AXI] [get_bd_intf_pins axi_clock_converter_0/S_AXI]
connect_bd_net [get_bd_pins axi_clock_converter_0/s_axi_aclk] [get_bd_pins ps_0/FCLK_CLK0]
connect_bd_net [get_bd_pins axi_clock_converter_0/s_axi_aresetn] [get_bd_pins rst_ps_0_142M/peripheral_aresetn]
connect_bd_net [get_bd_pins axi_clock_converter_0/m_axi_aclk] [get_bd_pins adc_dac_0/adc_clk_o]
connect_bd_net [get_bd_pins axi_clock_converter_0/m_axi_aresetn] [get_bd_pins adc_rst/dout]

create_bd_cell -type ip -vlnv pavel-demin:user:axi_cfg_register:1.0 axi_cfg_register_0
connect_bd_intf_net [get_bd_intf_pins axi_cfg_register_0/S_AXI] [get_bd_intf_pins axi_clock_converter_0/M_AXI]



connect_bd_net [get_bd_pins axi_cfg_register_0/aclk] [get_bd_pins axi_clock_converter_0/m_axi_aclk]
connect_bd_net [get_bd_pins axi_cfg_register_0/aresetn] [get_bd_pins adc_rst/dout]
connect_bd_net [get_bd_pins axi_cfg_register_0/cfg_data] [get_bd_pins pwm/threshold]
connect_bd_net [get_bd_pins pwm/rst] [get_bd_pins adc_rst/dout]

