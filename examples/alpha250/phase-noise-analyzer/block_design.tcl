set alpha250_mmcm_ps_control 1
source ${board_path}/starting_point.tcl
# MMCM reset and phase commands must remain accessible while its output stops.
disconnect_bd_net [get_bd_nets -of_objects [get_bd_pins adc_dac/ctl]] [get_bd_pins adc_dac/ctl]
connect_pins adc_dac/ctl [ps_ctl_pin mmcm_ps]
source $sdk_path/fpga/ip/awg_v1_0/integration.tcl

####################################
# Unmodulated local oscillators for phase extraction
####################################

for {set i 0} {$i < 2} {incr i} {

  # Noise_Shaping Taylor_Series_Corrected

  cell xilinx.com:ip:dds_compiler:6.0 dds$i {
    PartsPresent Phase_Generator_and_SIN_COS_LUT
    DDS_Clock_Rate [expr [get_parameter adc_clk] / 1000000.0]
    Parameter_Entry Hardware_Parameters
    Noise_Shaping None
    Phase_Width 48
    Output_Width [get_parameter dds_output_width]
    Phase_Increment Programmable
    Latency_Configuration Configurable
    Latency 9
  } {
    aclk adc_dac/adc_clk
  }

  cell pavel-demin:user:axis_constant:1.0 phase_increment$i {
    AXIS_TDATA_WIDTH 48
  } {
    cfg_data [get_concat_pin [list [ctl_pin phase_incr[expr 2*$i]] [get_slice_pin [ctl_pin phase_incr[expr 2*$i + 1]] 15 0]]]
    aclk adc_dac/adc_clk
    M_AXIS dds$i/S_AXIS_CONFIG
  }

}

# The DAC stimulus must have an independent phase path. Feeding its modulated
# carrier into the reference mixers would cancel the PM in a loopback test.
set outputs [dds_pm::add awg awg adc_dac/adc_clk [get_parameter adc_clk] \
    [dict create CHANNELS 2 OUTPUT_WIDTH [get_parameter dac_width]]]
for {set channel 0} {$channel < [llength $outputs]} {incr channel} {
    connect_pins adc_dac/dac$channel [lindex $outputs $channel]
}

####################################
# Phase extraction
####################################

source $project_path/tcl/cordic.tcl

cell koheron:user:phase_stream_control:1.0 phase_stream_control {} {
  aclk adc_dac/adc_clk
  aresetn rst_adc_clk/peripheral_aresetn
  requested_rate [ctl_pin cic_rate]
  requested_bits [get_slice_pin [ctl_pin phase_precision] 3 0]
  requested_epoch [get_slice_pin [ctl_pin phase_precision] 8 8]
  requested_run [get_slice_pin [ctl_pin acquisition_run] 0 0]
  sample_gap [sts_pin sample_gap]
}
cell xilinx.com:ip:util_vector_logic:2.0 phase_history_reset {
  C_SIZE 1 C_OPERATION not
} { Op1 phase_stream_control/filter_resetn }


set rounding_seeds {0x9e3779b97f4a7c15 0xd1b54a32d192ed03}
for {set i 0} {$i < 2} {incr i} {

    # Separate mixer and prefilter rounding sequences for the two ADC channels.
    cordic::create cordic$i [lindex $rounding_seeds $i]

    connect_cell cordic$i {
        s_axis_data_a [get_concat_pin [list adc_dac/adc$i [get_constant_pin 0 16]]]
        s_axis_data_b dds$i/m_axis_data_tdata
        s_axis_tvalid dds$i/m_axis_data_tvalid
        aclk adc_dac/adc_clk
        aresetn rst_adc_clk/peripheral_aresetn
        acc_on [get_slice_pin [ctl_pin cordic] $i $i]
        rst_phase phase_history_reset/Res
    }

  connect_pins cordic$i/demod [sts_pin demod$i]
}

####################################
# Monitor Phase with DMA
####################################

source $sdk_path/fpga/lib/pna_single_stream.tcl

# Repair short DAC paths after routing; refresh reports for strict timing checks.
set_property STRATEGY Performance_ExplorePostRoutePhysOpt [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.TCL.POST [file normalize [file join [file dirname [info script]] post_route.tcl]] [get_runs impl_1]
