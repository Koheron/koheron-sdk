set board_preset $board_path/config/board_preset.tcl
source $sdk_path/fpga/lib/starting_point.tcl

source $board_path/adc.tcl

# Add config and status registers
source $sdk_path/fpga/lib/ctl_sts.tcl
add_ctl_sts adc/adc_clk rst_adc_clk/peripheral_aresetn

connect_cell adc {
    ctl [ctl_pin mmcm]
    cfg_data [ps_ctl_pin spi_cfg_data]
    cfg_cmd [ps_ctl_pin spi_cfg_cmd]
    cfg_sts [ps_sts_pin spi_cfg_sts]
}

cell xilinx.com:ip:xlconcat:2.1 concat_interrupts {
  NUM_PORTS 1
} {
  dout ps_0/IRQ_F2P
}

####################################
# Direct Digital Synthesis
####################################

for {set i 0} {$i < 4} {incr i} {

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
    aclk adc/adc_clk
  }

  cell pavel-demin:user:axis_constant:1.0 phase_increment$i {
    AXIS_TDATA_WIDTH 48
  } {
    cfg_data [get_concat_pin [list [ctl_pin phase_incr[expr 2*$i]] [get_slice_pin [ctl_pin phase_incr[expr 2*$i + 1]] 15 0]]]
    aclk adc/adc_clk
    M_AXIS dds$i/S_AXIS_CONFIG
  }
}

####################################
# Phase extraction
####################################

source $project_path/tcl/cordic.tcl

cell xilinx.com:ip:util_vector_logic:2.0 filter_reset {
  C_SIZE 1 C_OPERATION not
} {}
cell xilinx.com:ip:util_vector_logic:2.0 phase_reset {
  C_SIZE 1 C_OPERATION or
} {
  Op1 [get_slice_pin [ctl_pin cordic] 1 1]
  Op2 filter_reset/Res
}

set adc_chans {00 01 10 11}

for {set i 0} {$i < 4} {incr i} {
    set adc_chan [lindex $adc_chans $i]

    # Separate initial states for mixer and prefilter rounding in each channel.
    set rounding_seeds {0x9e3779b97f4a7c15 0xd1b54a32d192ed03 0x94d049bb133111eb 0x8538ecb5bd456ea3}
    cordic::create cordic$i [lindex $rounding_seeds $i]

    connect_cell cordic$i {
        s_axis_data_a [get_concat_pin [list adc/adc$adc_chan [get_constant_pin 0 16]]]
        s_axis_data_b dds$i/m_axis_data_tdata
        s_axis_tvalid dds$i/m_axis_data_tvalid
        aclk adc/adc_clk
        aresetn rst_adc_clk/peripheral_aresetn
        acc_on [get_slice_pin [ctl_pin cordic] 0 0]
        rst_phase phase_reset/Res
    }

    connect_pins cordic$i/demod [sts_pin demod$i]

    cell xilinx.com:ip:mult_gen:12.0 scaler$i {
      PortAWidth [get_parameter phase_accumulator_width]
      PortBWidth 32
      PortAType Signed
      PortBType Signed
      OutputWidthHigh [expr [get_parameter phase_accumulator_width] + 29]
      OutputWidthLow 30
      Use_Custom_Output_Width true
      PipeStages 5
    } {
      A cordic$i/phase
      B [ctl_pin scaling$i]
      clk adc/adc_clk
    }
}

cell xilinx.com:ip:c_addsub:12.0 phase_diff0 {
  A_WIDTH [get_parameter phase_accumulator_width]
  B_WIDTH [get_parameter phase_accumulator_width]
  OUT_WIDTH [expr [get_parameter phase_accumulator_width] + 1]
  ADD_MODE Subtract
  CE false
} {
  A scaler0/P
  B scaler1/P
  clk adc/adc_clk
}

cell xilinx.com:ip:c_addsub:12.0 phase_diff1 {
  A_WIDTH [get_parameter phase_accumulator_width]
  B_WIDTH [get_parameter phase_accumulator_width]
  OUT_WIDTH [expr [get_parameter phase_accumulator_width] + 1]
  ADD_MODE Subtract
  CE false
} {
  A scaler2/P
  B scaler3/P
  clk adc/adc_clk
}

####################################
# Monitor Phase with DMA
####################################

source $sdk_path/fpga/lib/pna_filter.tcl
set_property XPM_LIBRARIES [lsort -unique [concat [get_property XPM_LIBRARIES [current_project]] XPM_CDC]] [current_project]

set_property -dict [list CONFIG.NUM_SI {3} CONFIG.NUM_MI {3}] [get_bd_cells axi_mem_intercon_1]

cell koheron:user:axis_stream_packet_mux:1.0 axis_stream_packet_m_0 {
} {
  S_AXI_LITE axi_mem_intercon_1/M00_AXI
  aclk ps_0/FCLK_CLK1
  aresetn proc_sys_reset_1/peripheral_aresetn
}

cell koheron:user:paired_cic_control:1.0 paired_cic_control {} {
  aclk adc/adc_clk
  aresetn rst_adc_clk/peripheral_aresetn
  requested_rate [ctl_pin cic_rate]
  requested_bits [get_slice_pin [ctl_pin phase_precision] 3 0]
  requested_epoch [get_slice_pin [ctl_pin phase_precision] 8 8]
  requested_run [get_slice_pin [ctl_pin acquisition_run] 0 0]
  sample_gap [sts_pin sample_gap]
}

cell koheron:user:phase_stream_cdc:1.0 phase_filter_cdc { METADATA_WIDTH 7 } {
  clk ps_0/FCLK_CLK1 status_clk adc/adc_clk
  resetn_in paired_cic_control/filter_resetn rate_in paired_cic_control/config_rate
  metadata_in [get_concat_pin [list paired_cic_control/active_bits paired_cic_control/overflow_x paired_cic_control/overflow_y paired_cic_control/sample_gap]]
  packet_in [get_constant_pin 0 32]
}

for {set i 0} {$i < 2} {incr i} {
  cell koheron:user:phase_range_guard:1.0 difference_range$i {
    INPUT_WIDTH [expr [get_parameter phase_accumulator_width] + 1]
    OUTPUT_WIDTH 32
  } {
    clk adc/adc_clk
    aresetn paired_cic_control/filter_resetn
    din phase_diff$i/S
  }
  set input_ready [pna_create_filter $i adc/adc_clk paired_cic_control/filter_resetn \
    difference_range$i/dout paired_cic_control/data_valid ps_0/FCLK_CLK1 \
    [expr {[get_parameter fclk1] / 1000000.}] phase_filter_cdc/resetn phase_filter_cdc/rate]
  connect_pins paired_cic_control/data_ready_[lindex {x y} $i] $input_ready

  cell xilinx.com:ip:util_vector_logic:2.0 cordic_overflow$i {
    C_SIZE 1 C_OPERATION or
  } {
    Op1 cordic[expr 2*$i]/overflow
    Op2 cordic[expr 2*$i+1]/overflow
  }
  cell xilinx.com:ip:util_vector_logic:2.0 upstream_overflow$i {
    C_SIZE 1 C_OPERATION or
  } {
    Op1 cordic_overflow$i/Res
    Op2 difference_range$i/overflow
  }
  cell koheron:user:phase_quantizer:1.0 phase_quantizer$i {
    PKT_LENGTH 8192
    BASE_SHIFT 8
  } {
    aclk ps_0/FCLK_CLK1
    aresetn phase_filter_cdc/resetn
    requested_bits [get_slice_pin phase_filter_cdc/metadata 3 0]
    upstream_overflow [get_slice_pin phase_filter_cdc/metadata [expr {4+$i}] [expr {4+$i}]]
    S_AXIS fir$i/M_AXIS_DATA
  }

  cell xilinx.com:ip:xlconcat:2.1 sample_metadata$i { NUM_PORTS 2 } {
    In0 phase_quantizer$i/sample_status
    In1 [get_slice_pin phase_filter_cdc/metadata 6 6]
  }
  cell koheron:user:phase_stream_cdc:1.0 fifo_count_cdc$i {} {
    clk ps_0/FCLK_CLK1 status_clk adc/adc_clk
    resetn_in paired_cic_control/filter_resetn
    rate_in paired_cic_control/config_rate
    metadata_in [get_constant_pin 0 6]
    packet_status [sts_pin fifo_wr_data_count$i]
  }
  cell xilinx.com:ip:axis_data_fifo:2.0 axis_data_fifo_$i {
    FIFO_DEPTH 32768
    TDATA_NUM_BYTES 4
    TUSER_WIDTH 6
    IS_ACLK_ASYNC 1
    HAS_PROG_FULL 1
    PROG_FULL_THRESH 16384
    HAS_WR_DATA_COUNT 1
  } {
    S_AXIS phase_quantizer$i/M_AXIS
    s_axis_tuser sample_metadata$i/dout
    s_axis_aclk ps_0/FCLK_CLK1
    s_axis_aresetn phase_filter_cdc/resetn
    m_axis_aclk ps_0/FCLK_CLK1
    M_AXIS axis_stream_packet_m_0/S_AXIS_$i
    prog_full [get_interrupt_pin]
    axis_wr_data_count fifo_count_cdc$i/packet_in
  }
}

# Separate FIFO drains must not let X/Y accept different live ADC samples.
# A shared rate source also keeps a runtime rate change on one sample epoch.
connect_pins paired_cic_control/filter_resetn filter_reset/Op1
connect_pins paired_cic_control/config_ready_x [get_constant_pin 1 1]
connect_pins paired_cic_control/config_ready_y [get_constant_pin 1 1]
for {set i 0} {$i < 2} {incr i} {
  connect_pins upstream_overflow$i/Res paired_cic_control/upstream_overflow_[lindex {x y} $i]
}

# set idx_dma [add_master_interface $intercon_idx]

cell xilinx.com:ip:axi_dma:7.1 axi_dma_0 {
  c_include_sg 1
  c_include_mm2s 0
  c_sg_include_stscntrl_strm 0
  c_sg_length_width 23
  c_s2mm_burst_size 16
} {
  S_AXIS_S2MM axis_stream_packet_m_0/M_AXIS
  S_AXI_LITE axi_mem_intercon_1/M01_AXI
  s_axi_lite_aclk ps_0/FCLK_CLK1
  M_AXI_S2MM axi_mem_intercon_1/S01_AXI
  M_AXI_SG axi_mem_intercon_1/S02_AXI
  m_axi_sg_aclk ps_0/FCLK_CLK1
  m_axi_s2mm_aclk ps_0/FCLK_CLK1
  axi_resetn proc_sys_reset_1/peripheral_aresetn
  s2mm_introut [get_interrupt_pin]
}

set_property -dict [list CONFIG.PCW_USE_S_AXI_HP0 {1} CONFIG.PCW_S_AXI_HP0_DATA_WIDTH {32}] [get_bd_cells ps_0]
connect_pins ps_0/S_AXI_HP0_ACLK ps_0/FCLK_CLK1
# set_property -dict [list CONFIG.NUM_SI {2} CONFIG.NUM_MI {2}] [get_bd_cells axi_mem_intercon_1]

#https://forums.xilinx.com/t5/Design-Entry/BD-41-237-Bus-Interface-property-ID-WIDTH-does-not-match/td-p/655028/page/2
set_property -dict [list CONFIG.STRATEGY {1}] [get_bd_cells axi_mem_intercon_1]

connect_bd_intf_net -boundary_type upper [get_bd_intf_pins axi_mem_intercon_1/M02_AXI] [get_bd_intf_pins ps_0/S_AXI_HP0]
connect_bd_net [get_bd_pins axi_mem_intercon_1/S02_ACLK] [get_bd_pins ps_0/FCLK_CLK1]
connect_bd_net [get_bd_pins axi_mem_intercon_1/S02_ARESETN] [get_bd_pins proc_sys_reset_1/peripheral_aresetn]

connect_bd_net [get_bd_pins axi_mem_intercon_1/S01_ACLK] [get_bd_pins ps_0/FCLK_CLK1]
connect_bd_net [get_bd_pins axi_mem_intercon_1/S01_ARESETN] [get_bd_pins proc_sys_reset_1/peripheral_aresetn]

connect_bd_net [get_bd_pins axi_mem_intercon_1/M00_ACLK] [get_bd_pins ps_0/FCLK_CLK1]
connect_bd_net [get_bd_pins axi_mem_intercon_1/M00_ARESETN] [get_bd_pins proc_sys_reset_1/peripheral_aresetn]

connect_bd_net [get_bd_pins axi_mem_intercon_1/M01_ACLK] [get_bd_pins ps_0/FCLK_CLK1]
connect_bd_net [get_bd_pins axi_mem_intercon_1/M01_ARESETN] [get_bd_pins proc_sys_reset_1/peripheral_aresetn]

connect_bd_net [get_bd_pins axi_mem_intercon_1/M02_ACLK] [get_bd_pins ps_0/FCLK_CLK1]
connect_bd_net [get_bd_pins axi_mem_intercon_1/M02_ARESETN] [get_bd_pins proc_sys_reset_1/peripheral_aresetn]

assign_bd_address [get_bd_addr_segs {axi_dma_0/S_AXI_LITE/Reg }]
set_property range [get_memory_range dma] [get_bd_addr_segs {ps_0/Data/SEG_axi_dma_0_Reg}]
set_property offset [get_memory_offset dma] [get_bd_addr_segs {ps_0/Data/SEG_axi_dma_0_Reg}]

assign_bd_address [get_bd_addr_segs {ps_0/S_AXI_HP0/HP0_DDR_LOWOCM }]
set_property range [get_memory_range ram] [get_bd_addr_segs {axi_dma_0/Data_S2MM/SEG_ps_0_HP0_DDR_LOWOCM}]
set_property offset [get_memory_offset ram] [get_bd_addr_segs {axi_dma_0/Data_S2MM/SEG_ps_0_HP0_DDR_LOWOCM}]

set_property range [get_memory_range ram] [get_bd_addr_segs {axi_dma_0/Data_SG/SEG_ps_0_HP0_DDR_LOWOCM}]
set_property offset [get_memory_offset ram] [get_bd_addr_segs {axi_dma_0/Data_SG/SEG_ps_0_HP0_DDR_LOWOCM}]

assign_bd_address -target_address_space /ps_0/Data [get_bd_addr_segs axis_stream_packet_m_0/s_axi/reg0] -force
set_property range [get_memory_range mux] [get_bd_addr_segs {ps_0/Data/SEG_axis_stream_packet_m_0_reg0}]
set_property offset [get_memory_offset mux] [get_bd_addr_segs {ps_0/Data/SEG_axis_stream_packet_m_0_reg0}]

delete_bd_objs [get_bd_addr_segs -excluded axi_dma_0/Data_S2MM/SEG_axi_dma_0_Reg]
delete_bd_objs [get_bd_addr_segs -excluded axi_dma_0/Data_SG/SEG_axi_dma_0_Reg]
delete_bd_objs [get_bd_addr_segs ps_0/Data/SEG_ps_0_HP0_DDR_LOWOCM]

# Physical optimization closes the phase extractor at 250 MHz.
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.ARGS.DIRECTIVE AggressiveExplore [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.TCL.POST [file normalize [file join [file dirname [info script]] post_route.tcl]] [get_runs impl_1]
