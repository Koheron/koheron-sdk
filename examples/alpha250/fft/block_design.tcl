source ${board_path}/starting_point.tcl

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

# Use the shared generator timing recovery after routing.
set_property STEPS.PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.PHYS_OPT_DESIGN.ARGS.DIRECTIVE AggressiveFanoutOpt [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.TCL.POST [file normalize $sdk_path/fpga/lib/post_route_hold_fix.tcl] [get_runs impl_1]
