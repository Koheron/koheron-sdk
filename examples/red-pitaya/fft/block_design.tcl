source $board_path/config/ports.tcl
source $board_path/base_system.tcl

# Hold unused board PWM outputs low.
connect_port_pin dac_pwm_o [get_constant_pin 0 4]

# Connect raw ADC data to status register
connect_cell adc_dac {
  adc1 [sts_pin adc0]
  adc2 [sts_pin adc1]
}

####################################
# Shared PNA DAC signal generator
####################################

source $sdk_path/fpga/ip/awg_v1_0/integration.tcl
set outputs [dds_pm::add awg awg adc_dac/adc_clk [get_parameter adc_clk] \
    [dict create CHANNELS 2]]
# Scale the 16-bit DAC stimulus to signed 14-bit at half amplitude (analog voltage depends on the load).
for {set channel 0} {$channel < [llength $outputs]} {incr channel} {
    connect_pins adc_dac/dac[expr {$channel+1}] [get_concat_pin [list [get_slice_pin [lindex $outputs $channel] 15 3] [get_slice_pin [lindex $outputs $channel] 15 15]] dac_scale$channel]
}

####################################
# Power Spectral Density
####################################

source $project_path/tcl/power_spectral_density.tcl
source $sdk_path/fpga/lib/bram_recorder.tcl

# The reference needs 89 DSPs here (80 available). Map butterfly arithmetic
# to LUTs; retain the reference complex multipliers and floating-point pipeline.
power_spectral_density::create psd [get_parameter fft_size] use_luts

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
  aresetn proc_sys_reset_adc_clk/peripheral_aresetn
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
  rst proc_sys_reset_adc_clk/peripheral_reset
  addr [get_concat_pin [list [get_constant_pin 0 2] [get_slice_pin bram_accum/m_axis_tuser 29 0]]]
  wen [get_concat_pin [lrepeat 4 bram_accum/m_axis_tvalid]]
  adc bram_accum/m_axis_tdata
}

# Match the reference AXI read interface and register the PS address path.
set_property CONFIG.PROTOCOL {AXI4} [get_bd_cells psd_bram/axi_bram_ctrl_psd]
set_property CONFIG.S00_HAS_REGSLICE 1 [get_bd_cells axi_mem_intercon_0]

# Use the reference post-route hold repair flow.
set_property STRATEGY Performance_NetDelay_high [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.PHYS_OPT_DESIGN.ARGS.DIRECTIVE AggressiveFanoutOpt [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.TCL.POST [file normalize $sdk_path/fpga/lib/post_route_hold_fix.tcl] [get_runs impl_1]
