# ADC ports
create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 adc_clk_in
set_property -dict [list CONFIG.FREQ_HZ [get_parameter adc_clk]] [get_bd_intf_ports adc_clk_in]
create_bd_port -dir I -from 6 -to 0 adc_0_p
create_bd_port -dir I -from 6 -to 0 adc_0_n
create_bd_port -dir I -from 6 -to 0 adc_1_p
create_bd_port -dir I -from 6 -to 0 adc_1_n

# Clock generator input
create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 clk_gen_in
set_property -dict [list CONFIG.FREQ_HZ [get_parameter adc_clk]] [get_bd_intf_ports clk_gen_in]

create_bd_port -dir O clk_gen_out_p
create_bd_port -dir O clk_gen_out_n

# DAC ports
create_bd_port -dir O -from 15 -to 0 dac_0
create_bd_port -dir O -from 15 -to 0 dac_1

# Configuration SPI

create_bd_port -dir O spi_cfg_sck
create_bd_port -dir O spi_cfg_sdi
create_bd_port -dir I spi_cfg_sdo

create_bd_port -dir O spi_cfg_cs_clk_gen ;# Clock generator
create_bd_port -dir O spi_cfg_cs_rf_dac
create_bd_port -dir O spi_cfg_cs_rf_adc

#---------------------------------------------------------------------------------------
# Start adc_dac IP
#---------------------------------------------------------------------------------------

set bd [current_bd_instance .]
current_bd_instance [create_bd_cell -type hier adc_dac]

for {set i 0} {$i < 2} {incr i} {
    create_bd_pin -dir I -from 15 -to 0 dac$i
    create_bd_pin -dir O -from 15 -to 0 dac${i}_out

    create_bd_pin -dir I -from 6 -to 0 adc_${i}_p
    create_bd_pin -dir I -from 6 -to 0 adc_${i}_n

    create_bd_pin -dir O -from 15 -to 0 adc${i}
}

# Control pin
create_bd_pin -dir I -from 31 -to 0 ctl
create_bd_pin -dir O pll_locked
create_bd_pin -dir I -from 31 -to 0 drp_ctl
create_bd_pin -dir O -from 31 -to 0 drp_sts

# Config SPI
create_bd_pin -dir I -from 31 -to 0 cfg_data
create_bd_pin -dir I -from 31 -to 0 cfg_cmd
create_bd_pin -dir O -from 31 -to 0 cfg_sts

create_bd_pin -dir O spi_cfg_sck
create_bd_pin -dir O spi_cfg_sdi
create_bd_pin -dir I spi_cfg_sdo

create_bd_pin -dir O spi_cfg_cs_clk_gen ;# Clock generator
create_bd_pin -dir O spi_cfg_cs_rf_dac
create_bd_pin -dir O spi_cfg_cs_rf_adc

# Input clocks
create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 clk_in1
create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 clk_in2
create_bd_pin -dir I ps_clk

# Output clocks
create_bd_pin -dir O adc_clk
create_bd_pin -dir O clk_gen_out_p
create_bd_pin -dir O clk_gen_out_n

set adc_clk_mhz [expr [get_parameter adc_clk] / 1000000.0]

# Vivado 2026.1 buffers only one of two differential inputs inside clk_wiz.
# Buffer both inputs here so the MMCM sees matching source types.
cell xilinx.com:ip:util_ds_buf:2.2 clk_in1_buf {
    C_BUF_TYPE IBUFDS
} {
    CLK_IN_D clk_in2
}
cell xilinx.com:ip:util_ds_buf:2.2 clk_in2_buf {
    C_BUF_TYPE IBUFDS
} {
    CLK_IN_D clk_in1
}

# With No_buffer inputs, clk_wiz leaves clock creation to the top level.
set input_clock_xdc [file join $output_path alpha250_input_clocks.xdc]
set input_clock_file [open $input_clock_xdc w]
set adc_clk_period [expr {1000000000.0 / [get_parameter adc_clk]}]
puts $input_clock_file [format {create_clock -name clk_gen_in -period %.6f [get_ports clk_gen_in_clk_p]} $adc_clk_period]
puts $input_clock_file [format {create_clock -name adc_clk_in -period %.6f [get_ports adc_clk_in_clk_p]} $adc_clk_period]
close $input_clock_file
add_files -norecurse -fileset constrs_1 $input_clock_xdc

# Reserve the worst DAC setup separation over the supported runtime rates.
# Keep the phase-zero hold requirement. Match the driver's MMCM divisors/phases.
set dac_phase_budget 0
set build_period [expr {1000000000.0 / [get_parameter adc_clk]}]
foreach {rate divide steps} {100000000 10 300 200000000 4 0 240000000 4 40 250000000 4 56} {
    if {$rate <= [get_parameter adc_clk]} {
        set phase_ns [expr {1000000000.0 * $steps / (56 * $rate * $divide)}]
        set budget [expr {$build_period - 1000000000.0 / $rate + $phase_ns}]
        set dac_phase_budget [expr {max($dac_phase_budget, ceil($budget * 1000) / 1000)}]
    }
}
if {$dac_phase_budget > 0} {
    set dac_phase_xdc [file join $output_path alpha250_dac_phase.xdc]
    set dac_phase_file [open $dac_phase_xdc w]
    puts $dac_phase_file [format {set_clock_uncertainty -setup %.3f -from [get_clocks -include_generated_clocks -of_objects [get_pins -hier *mmcm_adv*/CLKOUT0]] -to [get_clocks -include_generated_clocks -of_objects [get_pins -hier *mmcm_adv*/CLKOUT1]]} $dac_phase_budget]
    # Lower-rate clock models increase the DAC hold uncertainty by up to 17 ps.
    # Reserve 20 ps here, then check each runtime mode on the routed design.
    puts $dac_phase_file {set_clock_uncertainty -hold 0.020 -from [get_clocks -include_generated_clocks -of_objects [get_pins -hier *mmcm_adv*/CLKOUT0]] -to [get_clocks -include_generated_clocks -of_objects [get_pins -hier *mmcm_adv*/CLKOUT1]]}
    close $dac_phase_file
    add_files -norecurse -fileset constrs_1 $dac_phase_xdc
    set_property PROCESSING_ORDER LATE [get_files $dac_phase_xdc]
}

# The mailbox remains accessible while the sample clock is stopped/reset.
if {[get_parameter fclk0] > 200000000} {error "MMCM DRP requires fclk0 <= 200 MHz"}
cell koheron:user:mmcm_drp:1.0 drp {
} {
    aclk ps_clk
    ctl drp_ctl
    sts drp_sts
}

# Mixed-mode clock manager
set mmcm_divide [expr {[get_parameter adc_clk] == 100000000 ? 10 : 4}]
cell xilinx.com:ip:clk_wiz:6.0 mmcm {
    PRIMITIVE              MMCM
    PRIM_IN_FREQ.VALUE_SRC USER
    PRIM_IN_FREQ $adc_clk_mhz
    USE_INCLK_SWITCHOVER true
    SECONDARY_IN_FREQ      $adc_clk_mhz
    PRIM_SOURCE            No_buffer
    SECONDARY_SOURCE       No_buffer
    USE_INCLK_SWITCHOVER true
    OVERRIDE_MMCM true
    MMCM_DIVCLK_DIVIDE 1
    MMCM_CLKFBOUT_MULT_F $mmcm_divide
    MMCM_CLKOUT0_DIVIDE_F $mmcm_divide
    MMCM_CLKOUT1_DIVIDE $mmcm_divide
    MMCM_CLKFBOUT_USE_FINE_PS false
    CLKOUT1_USED true CLKOUT1_REQUESTED_OUT_FREQ $adc_clk_mhz CLKOUT1_REQUESTED_PHASE 0 CLK_OUT1_USE_FINE_PS_GUI true
    CLKOUT2_USED true CLKOUT2_REQUESTED_OUT_FREQ $adc_clk_mhz CLKOUT2_REQUESTED_PHASE 0
    USE_RESET true
    USE_DYN_RECONFIG true
    INTERFACE_SELECTION Enable_DRP
    USE_DYN_PHASE_SHIFT true
} {
    clk_in1 clk_in1_buf/IBUF_OUT
    clk_in2 clk_in2_buf/IBUF_OUT
    locked drp/locked
    clk_out1 adc_clk
    clk_in_sel [get_not_pin [get_slice_pin ctl 0 0]]
    reset drp/reset
    dclk ps_clk
    daddr drp/daddr
    den drp/den
    dwe drp/dwe
    din drp/di
    dout drp/dout
    drdy drp/drdy
    psclk mmcm/clk_out1
    psen [get_edge_detector_pin [get_slice_pin ctl 2 2] mmcm/clk_out1]
    psincdec [get_slice_pin ctl 3 3]
}

connect_pins pll_locked mmcm/locked

cell xilinx.com:ip:util_ds_buf:2.2 util_ds_buf_0 {
    C_BUF_TYPE OBUFDS
} {
    OBUF_IN mmcm/clk_out2
    OBUF_DS_P clk_gen_out_p
    OBUF_DS_N clk_gen_out_n
}

# ADC SelectIO
for {set i 0} {$i < 2} {incr i} {
    #
    cell xilinx.com:ip:selectio_wiz:5.1 selectio_adc$i {
        SELIO_ACTIVE_EDGE DDR
        SYSTEM_DATA_WIDTH 7
        SELIO_DDR_ALIGNMENT SAME_EDGE_PIPELINED
        BUS_SIG_TYPE DIFF
        BUS_IO_STD DIFF_HSTL_I_18
        SELIO_CLK_BUF MMCM
    } {
        clk_in mmcm/clk_out1
        data_in_from_pins_p adc_${i}_p
        data_in_from_pins_n adc_${i}_n
    }

    cell koheron:user:unrandomizer:1.0 unrandomizer$i {
        DATA_WIDTH 14
    } {
        din selectio_adc$i/data_in_to_device
    }

    if {[info exists adc_dac_extra_delay]} {
        connect_pin adc$i [get_Q_pin [get_concat_pin [list [get_constant_pin 0 2] unrandomizer$i/dout] concat_adc$i] $adc_dac_extra_delay noce mmcm/clk_out1]
    } else {
        connect_pin adc$i [get_concat_pin [list [get_constant_pin 0 2] unrandomizer$i/dout] concat_adc$i]
    }

}

# DAC SelectIO
# CLKOUT1 is already globally buffered; avoid a second regional clock buffer.
for {set i 0} {$i < 2} {incr i} {
    cell xilinx.com:ip:selectio_wiz:5.1 selectio_dac$i {
        BUS_DIR OUTPUTS
        SELIO_CLK_BUF MMCM
        BUS_IO_STD LVCMOS33
        SYSTEM_DATA_WIDTH 16
    } {
        clk_in mmcm/clk_out2
        data_out_to_pins dac${i}_out
    }

    if {[info exists adc_dac_extra_delay]} {
        connect_pins selectio_dac$i/data_out_from_device [get_Q_pin dac$i $adc_dac_extra_delay noce mmcm/clk_out1]
    } else {
        connect_pins selectio_dac$i/data_out_from_device dac$i
    }
}

# Configuration SPI
cell koheron:user:spi_cfg:1.0 spi_cfg_0 {
  CLK_DIV 3
  N_SLAVES 3
} {
  s_axis_tdata cfg_data
  s_axis_tvalid [get_slice_pin cfg_cmd 8 8]
  cmd [get_slice_pin cfg_cmd 7 0]
  s_axis_tready cfg_sts
  sclk spi_cfg_sck
  sdi spi_cfg_sdi
  aclk ps_clk
}

connect_pins spi_cfg_cs_clk_gen [get_slice_pin spi_cfg_0/cs 0 0]
connect_pins spi_cfg_cs_rf_dac [get_slice_pin spi_cfg_0/cs 1 1]
connect_pins spi_cfg_cs_rf_adc [get_slice_pin spi_cfg_0/cs 2 2]

current_bd_instance $bd

#---------------------------------------------------------------------------------------
# End adc_dac IP
#---------------------------------------------------------------------------------------

cell xilinx.com:ip:proc_sys_reset:5.0 rst_adc_clk {} {
  ext_reset_in ps_0/FCLK_RESET0_N
  slowest_sync_clk adc_dac/adc_clk
}

connect_cell adc_dac {
    clk_in1 adc_clk_in
    clk_in2 clk_gen_in
    ps_clk ps_0/FCLK_CLK0
    adc_0_p adc_0_p
    adc_0_n adc_0_n
    adc_1_p adc_1_p
    adc_1_n adc_1_n
    dac0_out dac_0
    dac1_out dac_1
    clk_gen_out_p clk_gen_out_p
    clk_gen_out_n clk_gen_out_n
    spi_cfg_sck spi_cfg_sck
    spi_cfg_sdi spi_cfg_sdi
    spi_cfg_sdo spi_cfg_sdo
    spi_cfg_cs_clk_gen spi_cfg_cs_clk_gen
    spi_cfg_cs_rf_dac spi_cfg_cs_rf_dac
    spi_cfg_cs_rf_adc spi_cfg_cs_rf_adc
}
