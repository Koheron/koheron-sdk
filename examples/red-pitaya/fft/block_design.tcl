source $board_path/config/ports.tcl
source $board_path/base_system.tcl

source $sdk_path/fpga/lib/laser_controller.tcl

# Connect raw ADC data to status register
connect_cell adc_dac {
  adc1 [sts_pin adc0]
  adc2 [sts_pin adc1]
}

####################################
# Direct Digital Synthesis
####################################

for {set i 0} {$i < 2} {incr i} {

  cell xilinx.com:ip:dds_compiler:6.0 dds$i {
    PartsPresent Phase_Generator_and_SIN_COS_LUT
    DDS_Clock_Rate [expr [get_parameter adc_clk] / 1000000.0]
    Parameter_Entry Hardware_Parameters
    Phase_Width 32
    Output_Width 16
    Phase_Increment Programmable
    Latency_Configuration Configurable
    Latency 9
  } {
    aclk adc_dac/adc_clk
  }

  connect_pins adc_dac/dac[expr $i+1] [get_slice_pin dds$i/m_axis_data_tdata 15 2]

  cell pavel-demin:user:axis_constant:1.0 phase_increment$i {
    AXIS_TDATA_WIDTH 32
  } {
    cfg_data [ctl_pin phase_incr$i]
    aclk adc_dac/adc_clk
    M_AXIS dds$i/S_AXIS_CONFIG
  }

}

####################################
# Power Spectral Density
####################################

source $project_path/tcl/power_spectral_density.tcl
source $sdk_path/fpga/modules/bram_accumulator/bram_accumulator.tcl
source $sdk_path/fpga/lib/bram_recorder.tcl

power_spectral_density::create psd [get_parameter fft_size]

cell koheron:user:latched_mux:1.0 mux_psd {
  N_INPUTS 2
  SEL_WIDTH 1
  WIDTH [get_parameter adc_width]
} {
  clk   adc_dac/adc_clk
  clken [get_constant_pin 1 1]
  sel   [ctl_pin psd_input_sel]
  din   [get_concat_pin [list adc_dac/adc1 adc_dac/adc2]]
}

connect_cell psd {
  data       mux_psd/dout
  clk        adc_dac/adc_clk
  tvalid     [ctl_pin psd_valid]
  ctl_fft    [ctl_pin ctl_fft]
}

# Accumulator
cell koheron:user:psd_counter:1.0 psd_counter {
  PERIOD [get_parameter fft_size]
  PERIOD_WIDTH [expr int(ceil(log([get_parameter fft_size]))/log(2))]
  N_CYCLES [get_parameter n_cycles]
  N_CYCLES_WIDTH [expr int(ceil(log([get_parameter n_cycles]))/log(2))]
} {
  clk           adc_dac/adc_clk
  s_axis_tvalid psd/m_axis_result_tvalid
  s_axis_tdata  psd/m_axis_result_tdata
  cycle_index   [sts_pin cycle_index]
}

bram_accumulator::create bram_accum
connect_cell bram_accum {
  clk adc_dac/adc_clk
  s_axis_tdata psd_counter/m_axis_tdata
  s_axis_tvalid psd_counter/m_axis_tvalid
  addr_in psd_counter/addr
  first_cycle psd_counter/first_cycle
  last_cycle psd_counter/last_cycle
}

# Record spectrum data in BRAM

add_bram_recorder psd_bram psd
connect_cell psd_bram {
  clk adc_dac/adc_clk
  rst proc_sys_reset_adc_clk/peripheral_reset
  addr bram_accum/addr_out
  wen bram_accum/wen
  adc bram_accum/m_axis_tdata
}

####################################
# Demodulation
####################################

source $project_path/tcl/demodulator.tcl

demodulator::create demodulator

connect_cell demodulator {
    s_axis_data_a [get_concat_pin [list [get_constant_pin 0 2] adc_dac/adc1 [get_constant_pin 0 16]]]
    s_axis_data_b dds0/m_axis_data_tdata
    s_axis_tvalid dds0/m_axis_data_tvalid
    aclk adc_dac/adc_clk
    aresetn proc_sys_reset_adc_clk/peripheral_aresetn
}

# Use AXI Stream clock converter (ADC clock -> FPGA clock)
set idx [add_master_interface 0]

cell xilinx.com:ip:axis_clock_converter:1.1 adc_clock_converter {
  TDATA_NUM_BYTES 4
} {
  s_axis_tdata demodulator/m_axis_tdata
  s_axis_tvalid demodulator/m_axis_tvalid
  s_axis_aresetn proc_sys_reset_adc_clk/peripheral_aresetn
  m_axis_aresetn proc_sys_reset_0/peripheral_aresetn
  s_axis_aclk adc_dac/adc_clk
  m_axis_aclk ps_0/FCLK_CLK0
}

# Add AXI stream FIFO
cell xilinx.com:ip:axi_fifo_mm_s:4.1 adc_axis_fifo {
  C_USE_TX_DATA 0
  C_USE_TX_CTRL 0
  C_USE_RX_CUT_THROUGH true
  C_RX_FIFO_DEPTH 8192
  C_RX_FIFO_PF_THRESHOLD 4096
} {
  s_axi_aclk ps_0/FCLK_CLK0
  s_axi_aresetn proc_sys_reset_0/peripheral_aresetn
  S_AXI axi_mem_intercon_0/M${idx}_AXI
  AXI_STR_RXD adc_clock_converter/M_AXIS
}

assign_bd_address [get_bd_addr_segs adc_axis_fifo/S_AXI/Mem0]
set memory_segment  [get_bd_addr_segs /ps_0/Data/SEG_adc_axis_fifo_Mem0]
set_property offset [get_memory_offset adc_fifo] $memory_segment
set_property range [get_memory_range adc_fifo] $memory_segment
