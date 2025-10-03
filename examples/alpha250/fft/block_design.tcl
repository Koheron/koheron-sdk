source ${board_path}/starting_point.tcl

####################################
# Direct Digital Synthesis
####################################

for {set i 0} {$i < 2} {incr i} {

  cell xilinx.com:ip:dds_compiler:6.0 dds$i {
    PartsPresent Phase_Generator_and_SIN_COS_LUT
    DDS_Clock_Rate [expr [get_parameter adc_clk] / 1000000]
    Parameter_Entry Hardware_Parameters
    Phase_Width 32
    Output_Width 16
    Phase_Increment Programmable
    Latency_Configuration Configurable
    Latency 9
  } {
    aclk adc_dac/adc_clk
  }

  connect_pins adc_dac/dac$i [get_slice_pin dds$i/m_axis_data_tdata 15 0]

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
  WIDTH 16
} {
  clk   adc_dac/adc_clk
  clken [get_constant_pin 1 1]
  sel   [ctl_pin psd_input_sel]
  din   [get_concat_pin [list adc_dac/adc0 adc_dac/adc1]]
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
  rst rst_adc_clk/peripheral_reset
  addr bram_accum/addr_out
  wen bram_accum/wen
  adc bram_accum/m_axis_tdata
}

set_property CONFIG.PROTOCOL {AXI4} [get_bd_cells psd_bram/axi_bram_ctrl_psd]

# Test IOs

connect_pins [sts_pin digital_inputs] [get_concat_pin [list exp_io_0_p exp_io_1_p exp_io_2_p exp_io_3_p exp_io_4_p exp_io_5_p exp_io_6_p exp_io_7_p]]

for {set i 0} {$i < 8} {incr i} {
    connect_pins  [get_slice_pin [ctl_pin digital_outputs] $i $i] exp_io_${i}_n
}
