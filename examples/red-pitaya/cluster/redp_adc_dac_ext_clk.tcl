proc add_redp_adc_dac {module_name} {
  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  for {set i 1} {$i <= 2} {incr i} {
    create_bd_pin -dir I -from 13 -to 0 dac$i
    create_bd_pin -dir O -from 13 -to 0 adc$i
  }

  create_bd_pin -dir I -from 31 -to 0 ctl
  create_bd_pin -dir I -type clk psclk
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 crystal_clk
  create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 sata_clk
  create_bd_pin -dir O adc_clk


  # Mixed-mode clock manager
  cell xilinx.com:ip:clk_wiz:6.0 mmcm {
    PRIMITIVE              MMCM
    PRIM_IN_FREQ.VALUE_SRC USER
    PRIM_IN_FREQ           125.0
    PRIM_SOURCE            Differential_clock_capable_pin
    MMCM_CLKFBOUT_USE_FINE_PS true
    CLKOUT1_USED true CLKOUT1_REQUESTED_OUT_FREQ 125.0 CLK_OUT1_USE_FINE_PS_GUI true
    CLKOUT2_USED true CLKOUT2_REQUESTED_OUT_FREQ 125.0
    CLKOUT3_USED true CLKOUT3_REQUESTED_OUT_FREQ 250.0
    CLKOUT4_USED true CLKOUT4_REQUESTED_OUT_FREQ 250.0 CLKOUT4_REQUESTED_PHASE -45
    USE_INCLK_SWITCHOVER true
    SECONDARY_IN_FREQ.VALUE_SRC USER
    SECONDARY_IN_FREQ 125
    USE_DYN_PHASE_SHIFT true
  } {
    clk_in_sel [get_slice_pin ctl 0 0]
    reset [get_slice_pin ctl 1 1]
    clk_out1  adc_clk
    CLK_IN2_D   crystal_clk
    CLK_IN1_D   sata_clk
    psclk psclk
    psen [get_edge_detector_pin [get_slice_pin ctl 2 2] psclk]
    psincdec [get_slice_pin ctl 3 3]
  }

  # Add ADC IP block
  cell pavel-demin:user:redp_adc:1.0 adc {} {
    adc_clk     mmcm/clk_out1
    adc_dat_a_o adc1
    adc_dat_b_o adc2
  }

  connect_ports adc

  # Add DAC IP block
  cell pavel-demin:user:redp_dac:1.0 dac {} {
    dac_dat_a_i dac1
    dac_dat_b_i dac2
    dac_clk_1x mmcm/clk_out2
    dac_clk_2x mmcm/clk_out3
    dac_clk_2p mmcm/clk_out4
    dac_locked mmcm/locked
  }

  connect_ports dac

  # Connect reset
  cell xilinx.com:ip:xlconstant:1.1 adc_rst {} {
    dout adc/adc_rst_i
  }

  current_bd_instance $bd
}
