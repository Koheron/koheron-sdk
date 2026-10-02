# Add PS and AXI Interconnect
set board_preset $board_path/config/board_preset.tcl
source $sdk_path/fpga/lib/starting_point.tcl

source $board_path/adc.tcl

# Add config and status registers
source $sdk_path/fpga/lib/ctl_sts.tcl
add_ctl_sts adc/adc_clk rst_adc_clk/peripheral_aresetn

connect_cell adc {
    adc00 [sts_pin adc00]
    adc01 [sts_pin adc01]
    adc10 [sts_pin adc10]
    adc11 [sts_pin adc11]
    ctl [ctl_pin mmcm]
    cfg_data [ps_ctl_pin spi_cfg_data]
    cfg_cmd [ps_ctl_pin spi_cfg_cmd]
    cfg_sts [ps_sts_pin spi_cfg_sts]
}

# Add XADC for monitoring of Zynq temperature

create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_analog_io_rtl:1.0 Vp_Vn

cell xilinx.com:ip:xadc_wiz:3.3 xadc_wiz_0 {
} {
  Vp_Vn Vp_Vn
  s_axi_lite axi_mem_intercon_0/M[add_master_interface 0]_AXI
  s_axi_aclk ps_0/FCLK_CLK0
  s_axi_aresetn proc_sys_reset_0/peripheral_aresetn
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
# Power Spectral Density
####################################

source $project_path/tcl/power_spectral_density.tcl
source $sdk_path/fpga/lib/bram_recorder.tcl

for {set j 0} {$j < 2} {incr j} { # ADC index

  power_spectral_density::create psd$j [get_parameter fft_size] $j

  cell koheron:user:latched_mux:1.0 mux_psd$j {
    N_INPUTS 2
    SEL_WIDTH 1
    WIDTH 16
  } {
    clk   adc/adc_clk
    clken [get_constant_pin 1 1]
    sel   [ctl_pin psd_input_sel$j]
    din   [get_concat_pin [list adc/adc${j}0 adc/adc${j}1]]
  }

  connect_cell psd$j {
    data       mux_psd$j/dout
    clk        adc/adc_clk
    tvalid     [ctl_pin psd_valid$j]
    ctl_fft    [ctl_pin ctl_fft$j]
  }

  # Accumulator
  # The real-time PSD producer cannot stall; allow a bank to drain before reuse.
  if {[get_parameter fft_size] < 16 || [get_parameter n_cycles] < 3} {
    error {Real-time PSD accumulation requires at least 16 bins and 3 frames}
  }
  cell koheron:user:axis_accumulator:1.0 bram_accum$j {
    FRAME_LENGTH [get_parameter fft_size]
    N_FRAMES [get_parameter n_cycles]
    CHECK_TLAST 1
    SYNC_ON_RESET 1
  } {
    aclk adc/adc_clk
    aresetn rst_adc_clk/peripheral_aresetn
    s_axis_tdata psd$j/m_axis_result_tdata
    s_axis_tvalid psd$j/m_axis_result_tvalid
    s_axis_tlast psd$j/m_axis_result_tlast
    m_axis_tready [get_constant_pin 1 1]
    cycle_index [sts_pin cycle_index$j]
  }

  # Record spectrum data in BRAM

  add_bram_recorder psd_bram$j psd$j
  connect_cell psd_bram$j {
    clk adc/adc_clk
    rst rst_adc_clk/peripheral_reset
    addr [get_concat_pin [list [get_constant_pin 0 2] [get_slice_pin bram_accum$j/m_axis_tuser 29 0]] psd_addr$j]
    wen [get_concat_pin [lrepeat 4 bram_accum$j/m_axis_tvalid] psd_wen$j]
    adc bram_accum$j/m_axis_tdata
  }
}

# Test IOs

connect_pins [sts_pin digital_inputs] [get_concat_pin [list exp_io_0_p exp_io_1_p exp_io_2_p exp_io_3_p exp_io_4_p exp_io_5_p exp_io_6_p exp_io_7_p]]

for {set i 0} {$i < 8} {incr i} {
    connect_pins  [get_slice_pin [ctl_pin digital_outputs] $i $i] exp_io_${i}_n
}

# Repair hold paths introduced by post-route setup optimization at the
# existing sample clock, as in the ALPHA250 FFT implementation.
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.ARGS.DIRECTIVE ExploreWithAggressiveHoldFix [get_runs impl_1]
