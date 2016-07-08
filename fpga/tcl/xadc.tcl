namespace eval xadc {


proc create {module_name} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:aximm_rtl:1.0 S_AXI
  create_bd_pin -type clk s_axi_aclk
  create_bd_pin -type rst s_axi_aresetn
  create_bd_pin -type clk m_axi_aclk
  create_bd_pin -type rst m_axi_aresetn

  foreach name {Vp_Vn Vaux0 Vaux1 Vaux8 Vaux9} {
    create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:diff_analog_io_rtl:1.0 $name
  }

  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:aximm_rtl:1.0 S_AXI_FIFO

  cell xilinx.com:ip:axi_clock_converter:2.1 axi_clk_conv_xadc {} {
    s_axi_aclk s_axi_aclk
    s_axi_aresetn s_axi_aresetn
    m_axi_aclk m_axi_aclk
    m_axi_aresetn m_axi_aresetn
  }
 
  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins axi_mem_intercon_$intercon_idx/M${idx}_AXI] [get_bd_intf_pins axi_clk_conv_xadc/S_AXI]

  cell xilinx.com:ip:xadc_wiz:3.3 xadc {
    XADC_STARUP_SELECTION        independent_adc
    CHANNEL_ENABLE_VAUXP0_VAUXN0 true
    CHANNEL_ENABLE_VAUXP1_VAUXN1 true
    CHANNEL_ENABLE_VAUXP8_VAUXN8 true
    CHANNEL_ENABLE_VAUXP9_VAUXN9 true
    CHANNEL_ENABLE_VP_VN         true
    DCLK_FREQUENCY               125
    ADC_CONVERSION_RATE          1000
    ENABLE_AXI4STREAM            true
  } {
    s_axi_aclk m_axi_aclk
    s_axi_aresetn m_axi_aresetn
    s_axi_lite axi_clk_conv_xadc/M_AXI
    Vp_Vn Vp_Vn
    Vaux0 Vaux0
    Vaux1 Vaux1
    Vaux8 Vaux8
    Vaux9 Vaux9
  }

  assign_bd_address [get_bd_addr_segs $name/s_axi_lite/Reg ]

  cell xilinx.com:ip:axis_clock_converter:1.1 fifo_clock_converter {
    TDATA_NUM_BYTES 4
  } {
    S_AXIS xadc/M_AXIS
    s_axis_aresetn m_axi_aresetn
    m_axis_aresetn s_axis_aresetn
    s_axis_aclk m_axi_aclk
    m_axis_aclk s_axi_aclk
  }

  cell xilinx.com:ip:axi_fifo_mm_s:4.1 xadc_axis_fifo {
    C_USE_TX_DATA 0
    C_USE_TX_CTRL 0
    C_USE_RX_CUT_THROUGH true
    C_RX_FIFO_DEPTH 4096
    C_RX_FIFO_PF_THRESHOLD 2048
  } {
    s_axi_aclk s_axi_aclk
    s_axi_aresetn s_axi_aresetn
    S_AXI S_AXI_FIFO
    AXI_STR_RXD fifo_clock_converter/M_AXIS
  }

  assign_bd_address [get_bd_addr_segs peak_axis_fifo/S_AXI/Mem0]
  set memory_segment [get_bd_addr_segs /${::ps_name}/Data/SEG_xadc_axis_fifo_Mem0]
  set_property offset $config::axi_xadc_fifo_offset $memory_segment
  set_property range $config::axi_xadc_fifo_range $memory_segment

  current_bd_instance $bd
}


} ;# end eval namespace
