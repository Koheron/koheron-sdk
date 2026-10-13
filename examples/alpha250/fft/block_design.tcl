source ${board_path}/starting_point.tcl

# Keep the first sample delay on the ADC clock and move the second to the DAC
# clock. Cross between fabric registers, before the tighter output pin register,
# while preserving the existing two-cycle delay.
current_bd_instance adc_dac
for {set channel 0} {$channel < 2} {incr channel} {
    delete_bd_objs [get_bd_cells Q_d2_noce_dac$channel]
    set adc_stage [get_Q_pin dac$channel 1 noce mmcm/clk_out1 dac_delay_adc$channel]
    set dac_stage [get_Q_pin $adc_stage 1 noce mmcm/clk_out2 dac_delay_dac$channel]
    connect_pins selectio_dac$channel/data_out_from_device $dac_stage
}
current_bd_instance /

# Register PS transactions before the AXI address decoder. The direct
# PS-to-crossbar decode path does not meet the 200 MHz fabric clock.
set_property CONFIG.S00_HAS_REGSLICE 1 [get_bd_cells axi_mem_intercon_0]

####################################
# Shared DAC signal generator (also used by PNA)
####################################

source $sdk_path/fpga/ip/awg_v1_0/integration.tcl
set outputs [dds_pm::add awg awg adc_dac/adc_clk [get_parameter adc_clk] \
    [dict create CHANNELS 2 OUTPUT_WIDTH [get_parameter dac_width]]]
for {set channel 0} {$channel < [llength $outputs]} {incr channel} {
    connect_pins adc_dac/dac$channel [lindex $outputs $channel]
}

####################################
# Power Spectral Density
####################################

source $project_path/tcl/power_spectral_density.tcl
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
# The real-time PSD producer cannot stall; allow a bank to drain before reuse.
if {[get_parameter fft_size] < 16 || [get_parameter n_cycles] < 3} {
  error {Real-time PSD accumulation requires at least 16 bins and 3 frames}
}
cell koheron:user:axis_accumulator:1.0 bram_accum {
  FRAME_LENGTH [get_parameter fft_size]
  N_FRAMES [get_parameter n_cycles]
  CHECK_TLAST 1
  SYNC_ON_RESET 1
} {
  aclk adc_dac/adc_clk
  aresetn rst_adc_clk/peripheral_aresetn
  s_axis_tdata psd/m_axis_result_tdata
  s_axis_tvalid psd/m_axis_result_tvalid
  s_axis_tlast psd/m_axis_result_tlast
  m_axis_tready [get_constant_pin 1 1]
  cycle_index [sts_pin cycle_index]
}

# Record spectrum data in BRAM

add_bram_recorder psd_bram psd
connect_cell psd_bram {
  clk adc_dac/adc_clk
  rst rst_adc_clk/peripheral_reset
  addr [get_concat_pin [list [get_constant_pin 0 2] [get_slice_pin bram_accum/m_axis_tuser 29 0]]]
  wen [get_concat_pin [lrepeat 4 bram_accum/m_axis_tvalid]]
  adc bram_accum/m_axis_tdata
}

set_property CONFIG.PROTOCOL {AXI4} [get_bd_cells psd_bram/axi_bram_ctrl_psd]

# Test IOs

connect_pins [sts_pin digital_inputs] [get_concat_pin [list exp_io_0_p exp_io_1_p exp_io_2_p exp_io_3_p exp_io_4_p exp_io_5_p exp_io_6_p exp_io_7_p]]

for {set i 0} {$i < 8} {incr i} {
    connect_pins  [get_slice_pin [ctl_pin digital_outputs] $i $i] exp_io_${i}_n
}

# Use the shared generator timing recovery after routing.
set_property STEPS.PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.PHYS_OPT_DESIGN.ARGS.DIRECTIVE AggressiveFanoutOpt [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.TCL.POST [file normalize $sdk_path/fpga/lib/post_route_hold_fix.tcl] [get_runs impl_1]
