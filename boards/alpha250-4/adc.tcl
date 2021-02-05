# ADC ports
create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 adc0_clk_in
create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 adc1_clk_in

set_property -dict [list CONFIG.FREQ_HZ [get_parameter adc_clk]] [get_bd_intf_ports adc0_clk_in]
set_property -dict [list CONFIG.FREQ_HZ [get_parameter adc_clk]] [get_bd_intf_ports adc1_clk_in]

create_bd_port -dir I -from 6 -to 0 adc0_0_p
create_bd_port -dir I -from 6 -to 0 adc0_0_n
create_bd_port -dir I -from 6 -to 0 adc0_1_p
create_bd_port -dir I -from 6 -to 0 adc0_1_n

create_bd_port -dir I -from 6 -to 0 adc1_0_p
create_bd_port -dir I -from 6 -to 0 adc1_0_n
create_bd_port -dir I -from 6 -to 0 adc1_1_p
create_bd_port -dir I -from 6 -to 0 adc1_1_n

# Clock generator input
create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 clk_gen_in
set_property -dict [list CONFIG.FREQ_HZ [get_parameter adc_clk]] [get_bd_intf_ports clk_gen_in]

create_bd_port -dir O clk_gen_out_p
create_bd_port -dir O clk_gen_out_n

# Configuration SPI

create_bd_port -dir O spi_cfg_sck
create_bd_port -dir O spi_cfg_sdi
create_bd_port -dir I spi_cfg_sdo

create_bd_port -dir O spi_cfg_cs_clk_gen ;# Clock generator
create_bd_port -dir O spi_cfg_cs_rf_adc0
create_bd_port -dir O spi_cfg_cs_rf_adc1

#---------------------------------------------------------------------------------------
# Start adc IP
#---------------------------------------------------------------------------------------

set bd [current_bd_instance .]
current_bd_instance [create_bd_cell -type hier adc]

for {set j 0} {$j < 2} {incr j} { # ADC index
    for {set i 0} {$i < 2} {incr i} { # Channel index
        create_bd_pin -dir I -from 6 -to 0 adc${j}_${i}_p
        create_bd_pin -dir I -from 6 -to 0 adc${j}_${i}_n

        create_bd_pin -dir O -from 15 -to 0 adc${j}${i}
    }
}

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
create_bd_pin -dir O spi_cfg_cs_rf_adc0
create_bd_pin -dir O spi_cfg_cs_rf_adc1

# Input clocks
create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 clk_in1
create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 clk_in2
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
    USE_INCLK_SWITCHOVER true
    SECONDARY_IN_FREQ      $adc_clk_mhz
    PRIM_SOURCE            Differential_clock_capable_pin
    USE_INCLK_SWITCHOVER true
    MMCM_CLKFBOUT_USE_FINE_PS true
    CLKOUT1_USED true CLKOUT1_REQUESTED_OUT_FREQ $adc_clk_mhz CLKOUT1_REQUESTED_PHASE 0 CLK_OUT1_USE_FINE_PS_GUI true
    CLKOUT2_USED true CLKOUT2_REQUESTED_OUT_FREQ $adc_clk_mhz CLKOUT2_REQUESTED_PHASE 0
    USE_RESET false
    USE_DYN_PHASE_SHIFT true
} {
    CLK_IN1_D clk_in1
    CLK_IN2_D clk_in2
    locked pll_locked
    clk_out1 adc_clk
    clk_in_sel [get_slice_pin ctl 0 0]
    reset [get_slice_pin ctl 1 1]
    psclk mmcm/clk_out1
    psen [get_edge_detector_pin [get_slice_pin ctl 2 2] mmcm/clk_out1]
    psincdec [get_slice_pin ctl 3 3]
}

cell xilinx.com:ip:util_ds_buf:2.1 util_ds_buf_0 {
    C_BUF_TYPE OBUFDS
} {
    OBUF_IN mmcm/clk_out2
    OBUF_DS_P clk_gen_out_p
    OBUF_DS_N clk_gen_out_n
}

# ADC SelectIO
for {set j 0} {$j < 2} {incr j} { # ADC index
    for {set i 0} {$i < 2} {incr i} { # Channel index
        #
        cell xilinx.com:ip:selectio_wiz:5.1 selectio_adc$j$i {
            SELIO_ACTIVE_EDGE DDR
            SYSTEM_DATA_WIDTH 7
            SELIO_DDR_ALIGNMENT SAME_EDGE_PIPELINED
            BUS_SIG_TYPE DIFF
            BUS_IO_STD DIFF_HSTL_I_18
            SELIO_CLK_BUF MMCM
        } {
            clk_in mmcm/clk_out1
            data_in_from_pins_p adc${j}_${i}_p
            data_in_from_pins_n adc${j}_${i}_n
        }

        cell koheron:user:unrandomizer:1.0 unrandomizer$j$i {
            DATA_WIDTH 14
        } {
            din selectio_adc$j$i/data_in_to_device
        }

        if {[info exists adc_dac_extra_delay]} {
            connect_pin adc$j$i [get_Q_pin [get_concat_pin [list [get_constant_pin 0 2] unrandomizer$j$i/dout] concat_adc$j$i] $adc_dac_extra_delay noce mmcm/clk_out1]
        } else {
            connect_pin adc$j$i [get_concat_pin [list [get_constant_pin 0 2] unrandomizer$j$i/dout] concat_adc$j$i]
        }
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
  sclk spi_cfg_sck
  sdi spi_cfg_sdi
  aclk ps_clk
}

connect_pins cfg_sts [get_concat_pin [list spi_cfg_0/s_axis_tready [get_constant_pin 0 31]]] 
connect_pins spi_cfg_cs_clk_gen [get_slice_pin spi_cfg_0/cs 0 0]
connect_pins spi_cfg_cs_rf_adc0 [get_slice_pin spi_cfg_0/cs 1 1]
connect_pins spi_cfg_cs_rf_adc1 [get_slice_pin spi_cfg_0/cs 2 2]

current_bd_instance $bd

#---------------------------------------------------------------------------------------
# End adc IP
#---------------------------------------------------------------------------------------

cell xilinx.com:ip:proc_sys_reset:5.0 rst_adc_clk {} {
  ext_reset_in ps_0/FCLK_RESET0_N
  slowest_sync_clk adc/adc_clk
}

# clk_in1 connected to adc1_clk_in.
# Since adc0_clk_in is not in the same clock region than clk_gen_in (and the MMCM),
# an error "sub-optimal placement fir a clock-capable io pin and mmcm pair" is generated.

connect_cell adc {
    clk_in1 adc1_clk_in
    clk_in2 clk_gen_in
    ps_clk ps_0/FCLK_CLK0
    adc0_0_p adc0_0_p
    adc0_0_n adc0_0_n
    adc0_1_p adc0_1_p
    adc0_1_n adc0_1_n
    adc1_0_p adc1_0_p
    adc1_0_n adc1_0_n
    adc1_1_p adc1_1_p
    adc1_1_n adc1_1_n
    clk_gen_out_p clk_gen_out_p
    clk_gen_out_n clk_gen_out_n
    spi_cfg_sck spi_cfg_sck
    spi_cfg_sdi spi_cfg_sdi
    spi_cfg_sdo spi_cfg_sdo
    spi_cfg_cs_clk_gen spi_cfg_cs_clk_gen
    spi_cfg_cs_rf_adc0 spi_cfg_cs_rf_adc0
    spi_cfg_cs_rf_adc1 spi_cfg_cs_rf_adc1
}
