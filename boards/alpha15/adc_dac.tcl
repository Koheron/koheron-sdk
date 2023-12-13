# ADC ports

# ADC data pins
create_bd_port -dir O adc0_clk_out_p
create_bd_port -dir O adc0_clk_out_n
create_bd_port -dir I adc0_dco_p
create_bd_port -dir I adc0_dco_n
create_bd_port -dir I adc0_da_p
create_bd_port -dir I adc0_da_n
create_bd_port -dir I adc0_db_p
create_bd_port -dir I adc0_db_n

create_bd_port -dir O adc1_clk_out_p
create_bd_port -dir O adc1_clk_out_n
create_bd_port -dir I adc1_dco_p
create_bd_port -dir I adc1_dco_n
create_bd_port -dir I adc1_da_p
create_bd_port -dir I adc1_da_n
create_bd_port -dir I adc1_db_p
create_bd_port -dir I adc1_db_n

# ADC control pins
create_bd_port -dir O adc0_ctl_range_sel
create_bd_port -dir O adc0_ctl_twolanes ;# Controls also ADC1
create_bd_port -dir O adc0_ctl_testpat
create_bd_port -dir O adc0_ctl_en

create_bd_port -dir O adc1_ctl_range_sel
create_bd_port -dir O adc1_ctl_testpat
create_bd_port -dir O adc1_ctl_en

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

# ADP5071 sync
create_bd_port -dir O adp5071_sync

#---------------------------------------------------------------------------------------
# Start adc_dac IP
#---------------------------------------------------------------------------------------

set bd [current_bd_instance .]
current_bd_instance [create_bd_cell -type hier adc_dac]

for {set i 0} {$i < 2} {incr i} {
    create_bd_pin -dir I -from 15 -to 0 dac$i
    create_bd_pin -dir O -from 15 -to 0 dac${i}_out

    create_bd_pin -dir I adc${i}_dco_p
    create_bd_pin -dir I adc${i}_dco_n
    create_bd_pin -dir I adc${i}_da_p
    create_bd_pin -dir I adc${i}_da_n
    create_bd_pin -dir I adc${i}_db_p
    create_bd_pin -dir I adc${i}_db_n

    create_bd_pin -dir I -from 4 -to 0 adc${i}_dco_delay_tap
    create_bd_pin -dir I -from 4 -to 0 adc${i}_da_delay_tap
    create_bd_pin -dir I -from 4 -to 0 adc${i}_db_delay_tap
    create_bd_pin -dir I adc${i}_delay_rst

    create_bd_pin -dir O -from 17 -to 0 adc${i}

    create_bd_pin -dir O adc${i}_clk_out_p
    create_bd_pin -dir O adc${i}_clk_out_n

    # DEBUG
    # create_bd_pin -dir O adc${i}_dco
    # create_bd_pin -dir O adc${i}_da
    # create_bd_pin -dir O adc${i}_db
}

create_bd_pin -dir I adc_clkout_dec

# ADC control
create_bd_pin -dir O adc_two_lane
create_bd_pin -dir O adc_valid

# Control pin
create_bd_pin -dir I -from 31 -to 0 ctl
create_bd_pin -dir O pll_locked

# Config SPI
create_bd_pin -dir I -from 31 -to 0 cfg_data
create_bd_pin -dir I -from 31 -to 0 cfg_cmd
create_bd_pin -dir O -from 31 -to 0 cfg_sts

create_bd_pin -dir O spi_cfg_sck
create_bd_pin -dir O spi_cfg_sdi
create_bd_pin -dir I spi_cfg_sdo

create_bd_pin -dir O spi_cfg_cs_clk_gen ;# Clock generator
create_bd_pin -dir O spi_cfg_cs_rf_dac

# ADP5071 sync
create_bd_pin -dir I adp5071_sync_en
create_bd_pin -dir I adp5071_sync_state
create_bd_pin -dir O adp5071_sync

# Input clocks
create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 clk_in
create_bd_pin -dir I ps_clk

# Output clocks
create_bd_pin -dir O adc_clk
create_bd_pin -dir O clk_gen_out_p
create_bd_pin -dir O clk_gen_out_n

set adc_clk_mhz [expr [get_parameter adc_clk] / 1000000]

# Mixed-mode clock manager
cell xilinx.com:ip:clk_wiz:5.4 mmcm {
    PRIMITIVE              MMCM
    PRIM_IN_FREQ.VALUE_SRC USER
    PRIM_IN_FREQ $adc_clk_mhz
    PRIM_SOURCE            Differential_clock_capable_pin
    USE_INCLK_SWITCHOVER false
    MMCM_CLKFBOUT_USE_FINE_PS true
    CLKOUT1_USED true CLKOUT1_REQUESTED_OUT_FREQ $adc_clk_mhz CLKOUT1_REQUESTED_PHASE 0 CLK_OUT1_USE_FINE_PS_GUI true
    CLKOUT2_USED true CLKOUT2_REQUESTED_OUT_FREQ $adc_clk_mhz CLKOUT2_REQUESTED_PHASE 0
    CLKOUT3_USED true CLKOUT3_REQUESTED_OUT_FREQ 200 CLKOUT3_REQUESTED_PHASE 0
    USE_RESET false
    USE_DYN_PHASE_SHIFT true
} {
    CLK_IN1_D clk_in
    locked pll_locked
    clk_out1 adc_clk
    psclk mmcm/clk_out1
    psen [get_edge_detector_pin [get_slice_pin ctl 2 2] mmcm/clk_out1]
    psincdec [get_slice_pin ctl 3 3]
}

# IDELAYCTRL requires a 200 MHz clock
cell xilinx.com:ip:util_idelay_ctrl:1.0 idelayctrl_0 {} {
    ref_clk mmcm/clk_out3
}

cell xilinx.com:ip:util_ds_buf:2.1 util_ds_buf_0 {
    C_BUF_TYPE OBUFDS
} {
    OBUF_IN mmcm/clk_out2
    OBUF_DS_P clk_gen_out_p
    OBUF_DS_N clk_gen_out_n
}

# ADC SelectIO

connect_pins adc_two_lane [get_constant_pin 1 1]

for {set i 0} {$i < 2} {incr i} {
    # The selectio delay must be enabled (even if no delay is set)
    # to instantiate IDELAYE primitives using the IDELAYCTRL reference

    cell xilinx.com:ip:selectio_wiz:5.1 selectio_adc_da_$i {
        BUS_SIG_TYPE DIFF
        BUS_IO_STD LVDS_25
        SELIO_ACTIVE_EDGE SDR
        USE_SERIALIZATION false
        SELIO_CLK_BUF MMCM
        SYSTEM_DATA_WIDTH 1
        SELIO_BUS_IN_DELAY VAR_LOADABLE
        INCLUDE_IDELAYCTRL false
    } {
        clk_in mmcm/clk_out1
        data_in_from_pins_p adc${i}_da_p
        data_in_from_pins_n adc${i}_da_n
        in_delay_tap_in adc${i}_da_delay_tap
        in_delay_reset  adc${i}_delay_rst
    }

    cell xilinx.com:ip:selectio_wiz:5.1 selectio_adc_db_$i {
        BUS_SIG_TYPE DIFF
        BUS_IO_STD LVDS_25
        SELIO_ACTIVE_EDGE SDR
        USE_SERIALIZATION false
        SELIO_CLK_BUF MMCM
        SYSTEM_DATA_WIDTH 1
        SELIO_BUS_IN_DELAY VAR_LOADABLE
        INCLUDE_IDELAYCTRL false
    } {
        clk_in mmcm/clk_out1
        data_in_from_pins_p adc${i}_db_p
        data_in_from_pins_n adc${i}_db_n
        in_delay_tap_in adc${i}_db_delay_tap
        in_delay_reset  adc${i}_delay_rst
    }

    cell xilinx.com:ip:selectio_wiz:5.1 selectio_adc_dco_$i {
        BUS_SIG_TYPE DIFF
        BUS_IO_STD LVDS_25
        SELIO_ACTIVE_EDGE SDR
        USE_SERIALIZATION false
        SELIO_CLK_BUF MMCM
        SYSTEM_DATA_WIDTH 1
        SELIO_BUS_IN_DELAY VAR_LOADABLE
        INCLUDE_IDELAYCTRL false
    } {
        clk_in mmcm/clk_out1
        data_in_from_pins_p adc${i}_dco_p
        data_in_from_pins_n adc${i}_dco_n
        in_delay_tap_in adc${i}_dco_delay_tap
        in_delay_reset  adc${i}_delay_rst
    }

    # DEBUG
    # connect_pins adc${i}_dco selectio_adc_dco_${i}/data_in_to_device
    # connect_pins adc${i}_da selectio_adc_da_${i}/data_in_to_device
    # connect_pins adc${i}_db selectio_adc_db_${i}/data_in_to_device
}

cell koheron:user:ltc2387:1.0 ltc2387 {} {
    clk mmcm/clk_out1
    din0_a selectio_adc_da_0/data_in_to_device
    din0_b selectio_adc_db_0/data_in_to_device
    din0_co selectio_adc_dco_0/data_in_to_device
    din1_a selectio_adc_da_1/data_in_to_device
    din1_b selectio_adc_db_1/data_in_to_device
    din1_co selectio_adc_dco_1/data_in_to_device
    clkout_dec adc_clkout_dec
    adc0 adc0
    adc1 adc1
    adc_valid adc_valid
}

for {set i 0} {$i < 2} {incr i} {
    cell xilinx.com:ip:selectio_wiz:5.1 selectio_clkout$i {
        BUS_DIR OUTPUTS
        BUS_SIG_TYPE DIFF
        BUS_IO_STD LVDS_25
        SELIO_CLK_BUF MMCM
        SYSTEM_DATA_WIDTH 1
    } {
        clk_in mmcm/clk_out1
        data_out_from_device ltc2387/clkout
        data_out_to_pins_p adc${i}_clk_out_p
        data_out_to_pins_n adc${i}_clk_out_n
    }
}

# DAC SelectIO
for {set i 0} {$i < 2} {incr i} {
    cell xilinx.com:ip:selectio_wiz:5.1 selectio_dac$i {
        BUS_DIR OUTPUTS
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

# ADP5071 Sync
cell koheron:user:dcdc_sync:1.0 dcdc_sync {
    DIVIDER [expr round([get_parameter adc_clk] / [get_parameter adp5071_clk])]
} {
    clk mmcm/clk_out1
    en adp5071_sync_en
    state_out adp5071_sync_state
    dcdc_clk adp5071_sync
}

current_bd_instance $bd

#---------------------------------------------------------------------------------------
# End adc_dac IP
#---------------------------------------------------------------------------------------

cell xilinx.com:ip:proc_sys_reset:5.0 rst_adc_clk {} {
  ext_reset_in ps_0/FCLK_RESET0_N
  slowest_sync_clk adc_dac/adc_clk
}

connect_cell adc_dac {
    clk_in clk_gen_in
    ps_clk ps_0/FCLK_CLK0
    adc_two_lane adc0_ctl_twolanes
    adc0_dco_p adc0_dco_p
    adc0_dco_n adc0_dco_n
    adc0_da_p adc0_da_p
    adc0_da_n adc0_da_n
    adc0_db_p adc0_db_p
    adc0_db_n adc0_db_n
    adc1_dco_p adc1_dco_p
    adc1_dco_n adc1_dco_n
    adc1_da_p adc1_da_p
    adc1_da_n adc1_da_n
    adc1_db_p adc1_db_p
    adc1_db_n adc1_db_n
    dac0_out dac_0
    dac1_out dac_1
    clk_gen_out_p clk_gen_out_p
    clk_gen_out_n clk_gen_out_n
    spi_cfg_sck spi_cfg_sck
    spi_cfg_sdi spi_cfg_sdi
    spi_cfg_sdo spi_cfg_sdo
    spi_cfg_cs_clk_gen spi_cfg_cs_clk_gen
    spi_cfg_cs_rf_dac spi_cfg_cs_rf_dac
    adp5071_sync adp5071_sync
    adc0_clk_out_p adc0_clk_out_p
    adc0_clk_out_n adc0_clk_out_n
    adc1_clk_out_p adc1_clk_out_p
    adc1_clk_out_n adc1_clk_out_n
}
