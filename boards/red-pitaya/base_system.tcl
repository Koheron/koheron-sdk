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

# Number of Master Interfaces in AXI Interconnect
set num_mi 1
set axi_interconnect_name ${ps_name}_axi_periph

# Add dual-port BRAM
set bram_name bram0
set bram_size 16K
set num_mi [expr $num_mi+1]
properties ps_0_axi_periph [list NUM_MI $num_mi]

create_bd_cell -type ip -vlnv xilinx.com:ip:axi_bram_ctrl:4.0 axi_bram_ctrl_$bram_name
create_bd_cell -type ip -vlnv xilinx.com:ip:blk_mem_gen:8.3 blk_mem_gen_$bram_name
properties blk_mem_gen_$bram_name {Memory_Type True_Dual_Port_RAM}
apply_bd_automation -rule xilinx.com:bd_rule:axi4 -config [list Master "/${ps_name}/M_AXI_GP0" Clk "Auto"] [get_bd_intf_pins axi_bram_ctrl_$bram_name/S_AXI]
connect_bd_intf_net [get_bd_intf_pins axi_bram_ctrl_$bram_name/BRAM_PORTA] [get_bd_intf_pins blk_mem_gen_$bram_name/BRAM_PORTA]
properties axi_bram_ctrl_$bram_name {SINGLE_PORT_BRAM 1}
set_property range $bram_size [get_bd_addr_segs $ps_name/Data/SEG_axi_bram_ctrl_${bram_name}_Mem0]

