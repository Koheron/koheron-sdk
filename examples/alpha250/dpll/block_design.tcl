# Add PS and AXI Interconnect
set board_preset $board_path/config/board_preset.tcl
source $sdk_path/fpga/lib/starting_point.tcl

source $board_path/adc_dac.tcl

# Add config and status registers
source $sdk_path/fpga/lib/ctl_sts.tcl
add_ctl_sts adc_dac/adc_clk rst_adc_clk/peripheral_aresetn
set_cell_props ctl/axi_ctl_register {PREDECODE_WRITES 1}

connect_cell adc_dac {
    adc0 [sts_pin adc0]
    adc1 [sts_pin adc1]
    ctl [ctl_pin mmcm]
    cfg_data [ps_ctl_pin spi_cfg_data]
    cfg_cmd [ps_ctl_pin spi_cfg_cmd]
    cfg_sts [ps_sts_pin spi_cfg_sts]
}

# Add XADC for monitoring of Zynq temperature

create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_analog_io_rtl:1.0 Vp_Vn

cell xilinx.com:ip:xlconcat:2.1 concat_interrupts {
  NUM_PORTS 1
} {
  dout ps_0/IRQ_F2P
}

cell xilinx.com:ip:xadc_wiz:3.3 xadc_wiz_0 {
} {
  Vp_Vn Vp_Vn
  s_axi_lite axi_mem_intercon_0/M[add_master_interface 0]_AXI
  s_axi_aclk ps_0/FCLK_CLK0
  s_axi_aresetn proc_sys_reset_0/peripheral_aresetn
  ip2intc_irpt concat_interrupts/In0
}
assign_bd_address [get_bd_addr_segs xadc_wiz_0/s_axi_lite/Reg]
set_property offset [get_memory_offset xadc] [get_bd_addr_segs {ps_0/Data/SEG_xadc_wiz_0_Reg}]

# Expansion connector IOs

for {set i 0} {$i < 8} {incr i} {
  create_bd_port -dir I exp_io_${i}_p
  create_bd_port -dir O exp_io_${i}_n
}

# SPI
source $board_path/spi.tcl

connect_pins ps_0/SDIO0_CDN [get_constant_pin 0 1]
connect_pins ps_0/SDIO0_WP [get_constant_pin 0 1]

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

source $project_path/tcl/cordic.tcl
source $project_path/tcl/corrector.tcl

create_bd_cell -type module -reference gain_programmer gain_programmer
connect_cell gain_programmer {
    clk adc_dac/adc_clk
    resetn rst_adc_clk/peripheral_aresetn
    cfg_command [ctl_pin gain_table_command]
    cfg_data [get_concat_pin [list [ctl_pin gain_table_data0] [ctl_pin gain_table_data1]]]
    ack [sts_pin gain_table_ack]
}
connect_pins [get_concat_pin [list gain_programmer/active_banks [get_constant_pin 0 24]]] [sts_pin gain_table_banks]
for {set word 0} {$word < 16} {incr word} {
    connect_pins [get_slice_pin gain_programmer/coefficients [expr 32*$word+31] [expr 32*$word]] [sts_pin gain_coefficients$word]
}

for {set i 0} {$i < 2} {incr i} {

    cordic::create cordic$i

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
        freq_in cordic$i/freq
        phase_in cordic$i/phase
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

set outputs [get_concat_pin [list \
    corrector0/fast_corr \
    corrector1/fast_corr \
    [get_concat_pin [list [get_slice_pin cordic0/phase 14 0] [get_slice_pin cordic0/phase 31 31]] "phase0_lsbs"] \
    [get_concat_pin [list [get_slice_pin cordic1/phase 14 0] [get_slice_pin cordic1/phase 31 31]] "phase1_lsbs"] \
    [get_slice_pin cordic0/phase 31 16] \
    [get_slice_pin cordic1/phase 31 16] \
    [get_slice_pin dds0/m_axis_data_tdata 15 0] \
    [get_slice_pin dds1/m_axis_data_tdata 15 0] \
]]

for {set i 0} {$i < 2} {incr i} {
    cell koheron:user:latched_mux:1.0 dac_mux$i {
       WIDTH 16
       N_INPUTS 8
       SEL_WIDTH 3
    } {
       clk adc_dac/adc_clk
       clken [get_constant_pin 1 1]
       din $outputs
       dout adc_dac/dac$i
       sel [get_slice_pin [ctl_pin dac_sel] [expr 3*$i+2] [expr 3*$i]]
    }
}

####################################
# Monitor Phase with DMA
####################################

# The monitor uses the exact shared PNA extractor, independently of the fast
# feedback detector. One selected ADC/DDS pair feeds its 24-bit mixer, fourth-
# order prefilter and fully pipelined 24-bit CORDIC.
source $sdk_path/fpga/lib/pna_cordic.tcl
cordic::create monitor_cordic 0x9e3779b97f4a7c15
foreach {name sources} {
    monitor_adc {adc_dac/adc0 adc_dac/adc1}
    monitor_dds {dds0/m_axis_data_tdata dds1/m_axis_data_tdata}
} {
    set width [expr {$name eq "monitor_adc" ? 16 : 32}]
    cell koheron:user:latched_mux:1.0 $name [list WIDTH $width N_INPUTS 2 SEL_WIDTH 1] {
        clk adc_dac/adc_clk clken [get_constant_pin 1 1]
        din [get_concat_pin $sources]
        sel [get_slice_pin [ctl_pin phase_sel] 0 0]
    }
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
cell xilinx.com:ip:util_vector_logic:2.0 monitor_phase_reset {
  C_SIZE 1 C_OPERATION not
} { Op1 phase_stream_control/filter_resetn }
connect_cell monitor_cordic {
    s_axis_data_a [get_concat_pin [list monitor_adc/dout [get_constant_pin 0 16]]]
    s_axis_data_b monitor_dds/dout
    s_axis_tvalid [get_constant_pin 1 1]
    aclk adc_dac/adc_clk
    aresetn rst_adc_clk/peripheral_aresetn
    acc_on [get_constant_pin 1 1]
    rst_phase monitor_phase_reset/Res
    demod [sts_pin monitor_demod]
}
set pna_phase_sources {monitor_cordic/phase}
set pna_overflow_sources {monitor_cordic/overflow}
set pna_phase_selector [get_slice_pin [ctl_pin phase_sel] 0 0]
source $sdk_path/fpga/lib/pna_single_stream.tcl

# Replicate timing-critical control nets without adding pipeline stages.
set_property STEPS.PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.PHYS_OPT_DESIGN.ARGS.DIRECTIVE AggressiveFanoutOpt [get_runs impl_1]
set_property STEPS.PLACE_DESIGN.ARGS.DIRECTIVE Explore [get_runs impl_1]
set_property STEPS.ROUTE_DESIGN.ARGS.DIRECTIVE Explore [get_runs impl_1]

# Repair short DAC paths after routing without adding pipeline registers.
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.ARGS.DIRECTIVE AggressiveExplore [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.TCL.POST [file normalize $project_path/tcl/post_route_opt.tcl] [get_runs impl_1]

# Keep DAC output registers near their physical handoff.
set_property STEPS.OPT_DESIGN.TCL.POST [file normalize $project_path/tcl/optimize_timing.tcl] [get_runs impl_1]

# Additional standalone RTL sources must not change automatic top selection.
set_property top system_wrapper [current_fileset]
