# Add PS and AXI Interconnect
set board_preset $board_path/config/board_preset.tcl
source $sdk_path/fpga/lib/starting_point.tcl

source $board_path/adc_dac.tcl

# Add config and status registers
source $sdk_path/fpga/lib/ctl_sts.tcl
add_ctl_sts adc_dac/adc_clk rst_adc_clk/peripheral_aresetn

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
    DDS_Clock_Rate [expr [get_parameter adc_clk] / 1000000]
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
        p_gain [ctl_pin p_gain$i]
        pi_gain [ctl_pin pi_gain$i]
        i2_gain [ctl_pin i2_gain$i]
        i3_gain [ctl_pin i3_gain$i]
        sclr [get_slice_pin [ctl_pin integrators$i] 3 1]
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

cell koheron:user:latched_mux:1.0 phase_mux {
    WIDTH 32
    N_INPUTS 2
    SEL_WIDTH 1
} {
    clk adc_dac/adc_clk
    clken [get_constant_pin 1 1]
    din [get_concat_pin [list cordic0/phase cordic1/phase]]
    sel [get_slice_pin [ctl_pin phase_sel] 0 0]
}

# Define CIC parameters

set diff_delay [get_parameter cic_differential_delay]
set dec_rate_default [get_parameter cic_decimation_rate_default]
set dec_rate_min [get_parameter cic_decimation_rate_min]
set dec_rate_max [get_parameter cic_decimation_rate_max]
set n_stages [get_parameter cic_n_stages]

cell xilinx.com:ip:cic_compiler:4.0 cic {
  Filter_Type Decimation
  Number_Of_Stages $n_stages
  Fixed_Or_Initial_Rate $dec_rate_default
  Sample_Rate_Changes Programmable
  Minimum_Rate $dec_rate_min
  Maximum_Rate $dec_rate_max
  Differential_Delay $diff_delay
  Input_Sample_Frequency [expr [get_parameter adc_clk] / 1000000.]
  Clock_Frequency [expr [get_parameter adc_clk] / 1000000.]
  Input_Data_Width 32
  Quantization Truncation
  Output_Data_Width 32
  Use_Xtreme_DSP_Slice false
  HAS_DOUT_TREADY true
} {
  aclk adc_dac/adc_clk
  s_axis_data_tdata phase_mux/dout
  s_axis_data_tvalid [get_constant_pin 1 1]
}

cell pavel-demin:user:axis_variable:1.0 cic_rate {
  AXIS_TDATA_WIDTH 16
} {
  cfg_data [ctl_pin cic_rate]
  aclk adc_dac/adc_clk
  aresetn rst_adc_clk/peripheral_aresetn
  M_AXIS cic/S_AXIS_CONFIG
}

set fir_coeffs [exec -- env -i $python -I fpga/scripts/fir.py $n_stages $dec_rate_default $diff_delay print]

cell xilinx.com:ip:fir_compiler:7.2 fir {
  Filter_Type Decimation
  Sample_Frequency [expr [get_parameter adc_clk] / 1000000. / $dec_rate_default]
  Clock_Frequency [expr [get_parameter fclk1] / 1000000.]
  Coefficient_Width 32
  Data_Width 32
  Output_Rounding_Mode Convergent_Rounding_to_Even
  Output_Width 32
  Decimation_Rate 2
  BestPrecision true
  CoefficientVector [subst {{$fir_coeffs}}]
  M_DATA_Has_TREADY true
} {
  aclk adc_dac/adc_clk
  S_AXIS_DATA cic/M_AXIS_DATA
}

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

cell xilinx.com:ip:axis_clock_converter:1.1 adc_clock_converter {
  TDATA_NUM_BYTES 4
} {
  S_AXIS fir/M_AXIS_DATA
  s_axis_aresetn rst_adc_clk/peripheral_aresetn
  m_axis_aresetn [set rst${intercon_idx}_name]/peripheral_aresetn
  s_axis_aclk adc_dac/adc_clk
  m_axis_aclk ps_0/FCLK_CLK1
}

cell koheron:user:tlast_gen:1.0 tlast_gen_0 {
  TDATA_WIDTH 32
  PKT_LENGTH [expr [get_parameter n_pts]]
} {
  aclk ps_0/FCLK_CLK1
  resetn proc_sys_reset_1/peripheral_aresetn
  s_axis adc_clock_converter/M_AXIS
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