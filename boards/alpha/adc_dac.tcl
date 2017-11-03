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
    create_bd_pin -dir I -from 15 -to 0 offset_adc${i}
}

# Control pin
create_bd_pin -dir I -from 31 -to 0 ctl

create_bd_pin -dir I -type clk psclk
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
create_bd_pin -dir O spi_cfg_cs_rf_adc

# Input clocks
create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 clk_in1
create_bd_intf_pin -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 clk_in2

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
    psclk psclk
    psen [get_edge_detector_pin [get_slice_pin ctl 2 2] psclk]
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

    cell xilinx.com:ip:xlconcat:2.1 concat_adc$i {
        NUM_PORTS 14
    } {
        dout adc$i
    }

    for {set j 0} {$j < 7} {incr j} {

        # Decode the digital output randomizer of the ADC
        set lsb [get_slice_pin selectio_adc$i/data_in_to_device 0 0]

        if {$j == 0} {
            connect_pins $lsb concat_adc$i/In0
        } else {
            cell xilinx.com:ip:util_vector_logic:2.0 xor_${i}_${j}_0 {
                C_SIZE 1
                C_OPERATION xor
            } {
                Op1 [get_slice_pin selectio_adc$i/data_in_to_device [expr $j] [expr $j]]
                Op2 $lsb
                Res concat_adc$i/In[expr 2*$j]
            }
        }

        cell xilinx.com:ip:util_vector_logic:2.0 xor_${i}_${j}_1 {
            C_SIZE 1
            C_OPERATION xor
        } {
            Op1 [get_slice_pin selectio_adc$i/data_in_to_device [expr $j + 7] [expr $j + 7]]
            Op2 $lsb
            Res concat_adc$i/In[expr 2*$j + 1]
        }
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
        data_out_from_device dac$i
        data_out_to_pins dac${i}_out
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
  aclk mmcm/clk_out1
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
