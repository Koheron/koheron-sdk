source $project_path/tcl/ports.tcl

# Add PS and AXI Interconnect
set board_preset $board_path/config/board_preset.tcl
source $sdk_path/fpga/lib/starting_point.tcl

# Add ADCs and DACs
source $sdk_path/fpga/lib/redp_adc_dac.tcl
set adc_dac_name adc_dac
add_redp_adc_dac $adc_dac_name

# Rename clocks
set adc_clk $adc_dac_name/adc_clk

# Add processor system reset synchronous to adc clock
set rst_adc_clk_name proc_sys_reset_adc_clk
cell xilinx.com:ip:proc_sys_reset:5.0 $rst_adc_clk_name {} {
  ext_reset_in $ps_name/FCLK_RESET0_N
  slowest_sync_clk $adc_clk
}

# Add config and status registers
source $sdk_path/fpga/lib/ctl_sts.tcl
add_ctl_sts $adc_clk $rst_adc_clk_name/peripheral_aresetn

# Connect LEDs
connect_port_pin led_o [get_slice_pin [ctl_pin led] 7 0]

# Connect ADC to status register
for {set i 0} {$i < [get_parameter n_adc]} {incr i} {
  connect_pins [sts_pin adc$i] adc_dac/adc[expr $i + 1]
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

####################################
# Direct Digital Synthesis
####################################

for {set i 0} {$i < 2} {incr i} {

  cell xilinx.com:ip:dds_compiler:6.0 dds$i {
    PartsPresent Phase_Generator_and_SIN_COS_LUT
    DDS_Clock_Rate [expr [get_parameter adc_clk] / 1000000]
    Parameter_Entry Hardware_Parameters
    Phase_Width 32
    Output_Width 14
    Phase_Increment Programmable
  } {
    aclk adc_dac/adc_clk
  }

  connect_pins adc_dac/dac[expr $i+1] [get_slice_pin dds$i/m_axis_data_tdata 13 0]

  cell pavel-demin:user:axis_variable:1.0 phase_increment$i {
    AXIS_TDATA_WIDTH 32
  } {
    cfg_data [ctl_pin phase_incr$i]
    aclk adc_dac/adc_clk
    aresetn $rst_adc_clk_name/peripheral_aresetn
    M_AXIS dds$i/S_AXIS_CONFIG
  }

}

####################################
# Power Spectral Density
####################################

source $project_path/tcl/power_spectral_density.tcl
source $sdk_path/fpga/modules/bram_accumulator/bram_accumulator.tcl
source $sdk_path/fpga/lib/bram_recorder.tcl

power_spectral_density::create psd [get_parameter fft_size] [get_parameter adc_width]

cell koheron:user:latched_mux:1.0 mux_psd {
  N_INPUTS 2
  SEL_WIDTH 1
  WIDTH 14
} {
  clk   adc_dac/adc_clk
  clken [get_constant_pin 1 1]
  sel   [ctl_pin psd_input_sel]
  din   [get_concat_pin [list adc_dac/adc1 adc_dac/adc2]]
}

connect_cell psd {
  adc1       mux_psd/dout
  adc2       [get_constant_pin 0 [expr [get_parameter adc_width] - 1]]
  clk        adc_dac/adc_clk
  tvalid     [ctl_pin psd_valid]
  ctl_sub    [ctl_pin substract_mean]
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
  rst $rst_adc_clk_name/peripheral_reset
  addr bram_accum/addr_out
  wen bram_accum/wen
  adc bram_accum/m_axis_tdata
}
