source $board_path/starting_point.tcl

# -----------------------------------------------------------------------------
# ADC
# -----------------------------------------------------------------------------

connect_pins [get_slice_pin [ctl_pin rf_adc_ctl0] 3 3] adc_dac/adc_clkout_dec
connect_pins [get_slice_pin [ctl_pin adp5071_sync] 0 0] adc_dac/adp5071_sync_en
connect_pins [get_slice_pin [ctl_pin adp5071_sync] 1 1] adc_dac/adp5071_sync_state

for {set i 0} {$i < 2} {incr i} {
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 0 0] adc${i}_ctl_range_sel
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 1 1] adc${i}_ctl_testpat
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 2 2] adc${i}_ctl_en

  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 8 4] adc_dac/adc${i}_dco_delay_tap
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 14 9] adc_dac/adc${i}_da_delay_tap
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 20 15] adc_dac/adc${i}_db_delay_tap
  connect_pins [get_slice_pin [ctl_pin rf_adc_ctl$i] 21 21] adc_dac/adc${i}_delay_rst
}

# Channel selection / addition / substraction
source $project_path/tcl/adc_selector.tcl
adc_selector::create adc_selector

connect_cell adc_selector {
  adc0           adc_dac/adc0
  adc1           adc_dac/adc1
  offset0        [ctl_pin channel_offset0]
  offset1        [ctl_pin channel_offset1]
  channel_select [ctl_pin channel_select]
  clk            adc_dac/adc_clk
  adc_valid      adc_dac/adc_valid
}

# -----------------------------------------------------------------------------
# Decimation
# -----------------------------------------------------------------------------

set intercon_idx 0

# Define CIC/FIR parameters
set diff_delay [get_parameter cic_differential_delay]
set dec_rate_default [get_parameter cic_decimation_rate_default]
set dec_rate_min [get_parameter cic_decimation_rate_min]
set dec_rate_max [get_parameter cic_decimation_rate_max]
set n_stages [get_parameter cic_n_stages]
set fir_coeffs [exec -- env -i $python -I fpga/scripts/fir.py $n_stages $dec_rate_min $diff_delay 128 0.45 print]

for {set i 0} {$i < 2} {incr i} {
  # Use AXI Stream clock converter (ADC clock -> PS clock)
  cell xilinx.com:ip:axis_clock_converter:1.1 adc_clock_converter$i {
    TDATA_NUM_BYTES 3
  } {
    s_axis_tdata   adc_selector/tdata
    s_axis_tvalid  adc_selector/tvalid
    s_axis_aresetn rst_adc_clk/peripheral_aresetn
    m_axis_aresetn [set rst${intercon_idx}_name]/peripheral_aresetn
    s_axis_aclk    adc_dac/adc_clk
    m_axis_aclk    [set ps_clk$intercon_idx]
  }

  cell xilinx.com:ip:cic_compiler:4.0 cic$i {
    Filter_Type Decimation
    Number_Of_Stages $n_stages
    Fixed_Or_Initial_Rate $dec_rate_default
    Sample_Rate_Changes Programmable
    Minimum_Rate $dec_rate_min
    Maximum_Rate $dec_rate_max
    Differential_Delay $diff_delay
    Input_Sample_Frequency 15
    Clock_Frequency [expr [get_parameter fclk0] / 1000000.]
    Input_Data_Width [get_parameter adc_width]
    Quantization Truncation
    Output_Data_Width 32
    Use_Xtreme_DSP_Slice false
  } {
    aclk        [set ps_clk$intercon_idx]
    S_AXIS_DATA adc_clock_converter$i/M_AXIS
  }

  cell pavel-demin:user:axis_variable:1.0 cic_rate$i {
    AXIS_TDATA_WIDTH 16
  } {
    cfg_data [ps_ctl_pin cic_rate$i]
    aclk [set ps_clk$intercon_idx]
    aresetn [set rst${intercon_idx}_name]/peripheral_aresetn
    M_AXIS cic$i/S_AXIS_CONFIG
  }

  cell xilinx.com:ip:fir_compiler:7.2 fir$i {
    Filter_Type Decimation
    Sample_Frequency [expr 15.0 / $dec_rate_min]
    Clock_Frequency [expr [get_parameter fclk0] / 1000000.]
    Coefficient_Width 32
    Data_Width 32
    Output_Rounding_Mode Convergent_Rounding_to_Even
    Output_Width 32
    Decimation_Rate 2
    BestPrecision true
    CoefficientVector [subst {{$fir_coeffs}}]
  } {
    aclk [set ps_clk$intercon_idx]
    S_AXIS_DATA cic$i/M_AXIS_DATA
  }

  set idx [add_master_interface $intercon_idx]
  # Add AXI stream FIFO
  cell xilinx.com:ip:axi_fifo_mm_s:4.1 adc_axis_fifo$i {
    C_USE_TX_DATA 0
    C_USE_TX_CTRL 0
    C_USE_RX_CUT_THROUGH true
    C_RX_FIFO_DEPTH 16384
    C_RX_FIFO_PF_THRESHOLD 8192
  } {
    s_axi_aclk    [set ps_clk$intercon_idx]
    s_axi_aresetn [set rst${intercon_idx}_name]/peripheral_aresetn
    S_AXI         [set interconnect_${intercon_idx}_name]/M${idx}_AXI
    AXI_STR_RXD   fir$i/M_AXIS_DATA
  }

  assign_bd_address   [get_bd_addr_segs adc_axis_fifo$i/S_AXI/Mem0]
  set memory_segment  [get_bd_addr_segs /${::ps_name}/Data/SEG_adc_axis_fifo${i}_Mem0]
  set_property offset [get_memory_offset adc_fifo$i] $memory_segment
  set_property range  [get_memory_range adc_fifo$i]  $memory_segment
}

# -----------------------------------------------------------------------------
# PSD
# -----------------------------------------------------------------------------

source $project_path/tcl/power_spectral_density.tcl
source $sdk_path/fpga/modules/bram_accumulator/bram_accumulator.tcl
source $sdk_path/fpga/lib/bram_recorder.tcl

power_spectral_density::create psd [get_parameter fft_size]

cell xilinx.com:ip:axis_clock_converter:1.1 psd_clock_converter {
  TDATA_NUM_BYTES 3
} {
  s_axis_tdata   adc_selector/tdata
  s_axis_tvalid  adc_selector/tvalid
  s_axis_aresetn rst_adc_clk/peripheral_aresetn
  m_axis_aresetn [set rst${intercon_idx}_name]/peripheral_aresetn
  s_axis_aclk    adc_dac/adc_clk
  m_axis_aclk    [set ps_clk$intercon_idx]
  m_axis_tready  [get_constant_pin 1 1]
}

connect_cell psd {
  data       psd_clock_converter/m_axis_tdata
  clk        [set ps_clk$intercon_idx]
  tvalid     psd_clock_converter/m_axis_tvalid
  ctl_fft    [ps_ctl_pin ctl_fft]
}

# Accumulator
cell koheron:user:psd_counter:1.0 psd_counter {
  PERIOD [get_parameter fft_size]
  PERIOD_WIDTH [expr int(ceil(log([get_parameter fft_size]))/log(2))]
  N_CYCLES [get_parameter n_cycles]
  N_CYCLES_WIDTH [expr int(ceil(log([get_parameter n_cycles]))/log(2))]
} {
  clk           [set ps_clk$intercon_idx]
  s_axis_tdata  psd/m_axis_result_tdata
  s_axis_tvalid psd/m_axis_result_tvalid
  cycle_index   [ps_sts_pin cycle_index]
}

bram_accumulator::create bram_accum
connect_cell bram_accum {
  clk           [set ps_clk$intercon_idx]
  s_axis_tdata  psd_counter/m_axis_tdata
  s_axis_tvalid psd_counter/m_axis_tvalid
  addr_in       psd_counter/addr
  first_cycle   psd_counter/first_cycle
  last_cycle    psd_counter/last_cycle
}

# Record spectrum data in BRAM

add_bram_recorder psd_bram psd
connect_cell psd_bram {
  clk  [set ps_clk$intercon_idx]
  rst  [set rst${intercon_idx}_name]/peripheral_reset
  addr bram_accum/addr_out
  wen  bram_accum/wen
  adc  bram_accum/m_axis_tdata
}

##################################################
# DMA
##################################################

set_property -dict [list CONFIG.PCW_USE_S_AXI_HP0 {1} CONFIG.PCW_S_AXI_HP0_DATA_WIDTH {32}] [get_bd_cells ps_0]
connect_pins ps_0/S_AXI_HP0_ACLK ps_0/FCLK_CLK1
set_property -dict [list CONFIG.NUM_SI {2} CONFIG.NUM_MI {2}] [get_bd_cells axi_mem_intercon_1]

#https://forums.xilinx.com/t5/Design-Entry/BD-41-237-Bus-Interface-property-ID-WIDTH-does-not-match/td-p/655028/page/2
set_property -dict [list CONFIG.STRATEGY {1}] [get_bd_cells axi_mem_intercon_1]

#connect_pins ps_0/FCLK_CLK1 axi_mem_intercon_1/M00_ACLK
#connect_pins axi_mem_intercon_1/M00_ARESETN proc_sys_reset_1/peripheral_aresetn
connect_bd_intf_net -boundary_type upper [get_bd_intf_pins axi_mem_intercon_1/M01_AXI] [get_bd_intf_pins ps_0/S_AXI_HP0]
connect_bd_net [get_bd_pins axi_mem_intercon_1/S01_ACLK] [get_bd_pins ps_0/FCLK_CLK1]
connect_bd_net [get_bd_pins axi_mem_intercon_1/S01_ARESETN] [get_bd_pins proc_sys_reset_1/peripheral_aresetn]

# Use AXI Stream clock converter (ADC clock -> FPGA clock)
set intercon_idx 1
set idx [add_master_interface $intercon_idx]

cell xilinx.com:ip:axis_clock_converter:1.1 dma_clock_converter {
  TDATA_NUM_BYTES 4
} {
  s_axis_tdata adc_selector/tdata
  s_axis_tvalid adc_selector/tvalid
  s_axis_aresetn rst_adc_clk/peripheral_aresetn
  m_axis_aresetn [set rst${intercon_idx}_name]/peripheral_aresetn
  s_axis_aclk adc_dac/adc_clk
  m_axis_aclk ps_0/FCLK_CLK1
}

cell koheron:user:tlast_gen:1.0 tlast_gen_0 {
  TDATA_WIDTH 32
  PKT_LENGTH [expr 1024*1024]
} {
  aclk ps_0/FCLK_CLK1
  resetn proc_sys_reset_1/peripheral_aresetn
  s_axis dma_clock_converter/M_AXIS
}

cell xilinx.com:ip:axi_dma:7.1 axi_dma_0 {
  c_include_sg 0
  c_include_mm2s 0
  c_sg_include_stscntrl_strm 0
  c_sg_length_width 23
  c_s2mm_burst_size 16
} {
  S_AXIS_S2MM tlast_gen_0/m_axis
  S_AXI_LITE axi_mem_intercon_1/M00_AXI
  s_axi_lite_aclk ps_0/FCLK_CLK1
  M_AXI_S2MM axi_mem_intercon_1/S01_AXI
  m_axi_s2mm_aclk ps_0/FCLK_CLK1
  axi_resetn proc_sys_reset_1/peripheral_aresetn
  s2mm_introut [get_interrupt_pin]
}

connect_bd_net [get_bd_pins axi_mem_intercon_1/M01_ACLK] [get_bd_pins ps_0/FCLK_CLK1]
connect_bd_net [get_bd_pins axi_mem_intercon_1/M01_ARESETN] [get_bd_pins proc_sys_reset_1/peripheral_aresetn]

assign_bd_address [get_bd_addr_segs {axi_dma_0/S_AXI_LITE/Reg }]
set_property range [get_memory_range dma] [get_bd_addr_segs {ps_0/Data/SEG_axi_dma_0_Reg}]
set_property offset [get_memory_offset dma] [get_bd_addr_segs {ps_0/Data/SEG_axi_dma_0_Reg}]

assign_bd_address [get_bd_addr_segs {ps_0/S_AXI_HP0/HP0_DDR_LOWOCM }]
set_property range [get_memory_range ram] [get_bd_addr_segs {axi_dma_0/Data_S2MM/SEG_ps_0_HP0_DDR_LOWOCM}]
set_property offset [get_memory_offset ram] [get_bd_addr_segs {axi_dma_0/Data_S2MM/SEG_ps_0_HP0_DDR_LOWOCM}]

delete_bd_objs [get_bd_addr_segs -excluded axi_dma_0/Data_S2MM/SEG_axi_dma_0_Reg]
delete_bd_objs [get_bd_addr_segs ps_0/Data/SEG_ps_0_HP0_DDR_LOWOCM]