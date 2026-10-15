# Shared DPLL datapath. Board scripts supply platform, clocks and DMA wiring.
set dpll_path [file normalize [file join [file dirname [info script]] ..]]
if {![info exists dpll_program_clock]} {set dpll_program_clock ps_0/FCLK_CLK1}
if {![info exists dpll_dac_mute]} {set dpll_dac_mute 0}
if {![info exists dpll_acquisition_tcl]} {set dpll_acquisition_tcl $sdk_path/fpga/lib/pna_single_stream.tcl}
if {![info exists dpll_optimize_timing]} {set dpll_optimize_timing $dpll_path/tcl/optimize_timing.tcl}

####################################
# Direct Digital Synthesis
####################################

for {set i 0} {$i < 2} {incr i} {

  cell xilinx.com:ip:dds_compiler:6.0 dds$i {
    PartsPresent Phase_Generator_and_SIN_COS_LUT
    DDS_Clock_Rate [expr [get_parameter adc_clk] / 1000000.0]
    Parameter_Entry Hardware_Parameters
    Phase_Width 48
    Output_Width 16
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

####################################
# Digital PLL
####################################

source $dpll_path/tcl/split_detector.tcl
source $dpll_path/tcl/corrector.tcl

create_bd_cell -type module -reference gain_programmer gain_programmer
connect_cell gain_programmer {
    clk adc_dac/adc_clk
    resetn rst_adc_clk/peripheral_aresetn
    program_clk $dpll_program_clock
    cfg_command [ctl_pin gain_table_command]
    cfg_data [get_concat_pin [list [ctl_pin gain_table_data0] [ctl_pin gain_table_data1]]]
    ack [sts_pin gain_table_ack]
}
connect_pins [get_concat_pin [list gain_programmer/active_banks [get_constant_pin 0 24]]] [sts_pin gain_table_banks]
for {set word 0} {$word < 16} {incr word} {
    connect_pins [get_slice_pin gain_programmer/coefficients [expr 32*$word+31] [expr 32*$word]] [sts_pin gain_coefficients$word]
}

for {set i 0} {$i < 2} {incr i} {

    split_detector::create cordic$i [format 0x%016llx [expr {0x9e3779b97f4a7c15 ^ ($i*0x517cc1b727220a95)}]]

    connect_cell cordic$i {
        s_axis_data_a [get_concat_pin [list adc_dac/adc$i [get_constant_pin 0 16]]]
        s_axis_data_b dds$i/m_axis_data_tdata
        s_axis_tvalid dds$i/m_axis_data_tvalid
        aclk adc_dac/adc_clk
        aresetn rst_adc_clk/peripheral_aresetn
        acc_on [get_slice_pin [ctl_pin integrators$i] 0 0]
    }

    corrector::create corrector$i

    connect_cell corrector$i {
        clk adc_dac/adc_clk
        resetn rst_adc_clk/peripheral_aresetn
        freq_in cordic$i/freq
        phase_in cordic$i/phase
        i_in cordic$i/i_filtered
        q_in cordic$i/q_filtered
        cx [ctl_pin p_fast_coeff[expr 2*$i]]
        cy [ctl_pin p_fast_coeff[expr 2*$i+1]]
        request_fast [get_slice_pin [ctl_pin p_path_control] $i $i]
        snapshot_request [get_slice_pin [ctl_pin p_path_control] [expr 8+$i] [expr 8+$i]]
        snapshot [sts_pin p_snapshot$i]
        path_status [sts_pin p_path_status$i]
        active_banks [get_slice_pin gain_programmer/active_banks [expr 4*$i+3] [expr 4*$i]]
        table_command gain_programmer/command$i
        table_data gain_programmer/data
        enabled [get_slice_pin [ctl_pin integrators$i] 3 1]
    }

}

# Connect slow_corr to precision DAC
delete_bd_objs [get_bd_nets ctl_precision_dac_data0]
connect_pins [get_concat_pin [list corrector0/slow_corr corrector1/slow_corr]] concat_precision_dac_data/In0

# For each DAC, choose between:
# 0: fast_corr0
# 1: fast_corr1
# 2: phase0 (16 LSBs)
# 3: phase1 (16 LSBs)
# 4: phase0 (16 MSBs)
# 5: phase1 (16 MSBs)
# 6: DDS0
# 7: DDS1

set output_sources [list \
    corrector0/fast_corr \
    corrector1/fast_corr \
    [get_concat_pin [list [get_slice_pin cordic0/phase 22 8] [get_slice_pin cordic0/phase 39 39]] "phase0_lsbs"] \
    [get_concat_pin [list [get_slice_pin cordic1/phase 22 8] [get_slice_pin cordic1/phase 39 39]] "phase1_lsbs"] \
    [get_slice_pin cordic0/phase 39 24] \
    [get_slice_pin cordic1/phase 39 24] \
    [get_slice_pin dds0/m_axis_data_tdata 15 0] \
    [get_slice_pin dds1/m_axis_data_tdata 15 0] \
]
if {$dpll_dac_mute} {lappend output_sources [get_constant_pin 0 16]}
set outputs [get_concat_pin $output_sources]
set dpll_route_width [expr {$dpll_dac_mute ? 4 : 3}]

for {set i 0} {$i < 2} {incr i} {
    cell koheron:user:latched_mux:1.0 dac_mux$i {
       WIDTH 16
       N_INPUTS [llength $output_sources]
       SEL_WIDTH $dpll_route_width
       OUTPUT_STAGES 2
    } {
       clk adc_dac/adc_clk
       clken [get_constant_pin 1 1]
       din $outputs
       dout adc_dac/dac$i
       sel [get_slice_pin [ctl_pin dac_sel] [expr $dpll_route_width*($i+1)-1] [expr $dpll_route_width*$i]]
    }
}

####################################
# Monitor Phase with DMA
####################################

# Both loops share accurate phase extraction with the monitor. Selection and
# epoch resets are downstream of extraction and cannot reset feedback state.
cell koheron:user:latched_mux:1.0 monitor_phase_mux {WIDTH 64 N_INPUTS 2 SEL_WIDTH 1} {
    clk adc_dac/adc_clk clken [get_constant_pin 1 1]
    din [get_concat_pin [list cordic0/monitor_phase cordic1/monitor_phase]]
    sel [get_slice_pin [ctl_pin phase_sel] 0 0]
}
cell koheron:user:latched_mux:1.0 monitor_demod_mux {WIDTH 32 N_INPUTS 2 SEL_WIDTH 1} {
    clk adc_dac/adc_clk clken [get_constant_pin 1 1]
    din [get_concat_pin [list cordic0/demod cordic1/demod]]
    sel [get_slice_pin [ctl_pin phase_sel] 0 0]
    dout [sts_pin monitor_demod]
}
cell koheron:user:phase_stream_control:1.0 phase_stream_control { RATE_STEP 2 } {
  aclk adc_dac/adc_clk
  aresetn rst_adc_clk/peripheral_aresetn
  requested_rate [ctl_pin cic_rate]
  requested_bits [get_slice_pin [ctl_pin phase_precision] 3 0]
  requested_epoch [get_slice_pin [ctl_pin phase_precision] 8 8]
  requested_run [get_slice_pin [ctl_pin acquisition_run] 0 0]
  sample_gap [sts_pin sample_gap]
}
create_bd_cell -type module -reference monitor_phase_origin monitor_origin
connect_cell monitor_origin {
    clk adc_dac/adc_clk resetn phase_stream_control/filter_resetn
    phase monitor_phase_mux/dout
}
set pna_phase_sources {monitor_origin/relative_phase}
set pna_overflow_sources {monitor_origin/overflow}
set pna_phase_selector [get_slice_pin [ctl_pin phase_sel] 0 0]
source $dpll_acquisition_tcl

# Replicate timing-critical control nets without adding pipeline stages.
set_property STEPS.PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.PHYS_OPT_DESIGN.ARGS.DIRECTIVE AggressiveFanoutOpt [get_runs impl_1]
set_property STEPS.PHYS_OPT_DESIGN.TCL.POST [file normalize $dpll_path/tcl/replicate_gain_control.tcl] [get_runs impl_1]
set_property STEPS.PLACE_DESIGN.ARGS.DIRECTIVE Explore [get_runs impl_1]
set_property STEPS.ROUTE_DESIGN.ARGS.DIRECTIVE Explore [get_runs impl_1]

# Repair short DAC paths after routing without adding pipeline registers.
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.ARGS.DIRECTIVE AggressiveExplore [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.TCL.POST [file normalize $dpll_path/tcl/post_route_opt.tcl] [get_runs impl_1]

# Keep DAC output registers near their physical handoff.
set_property STEPS.OPT_DESIGN.TCL.POST [file normalize $dpll_optimize_timing] [get_runs impl_1]

# Additional standalone RTL sources must not change automatic top selection.
set_property top system_wrapper [current_fileset]
