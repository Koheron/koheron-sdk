# Koheron ALPHA15 constraints file

set_property CFGBVS VCCO [current_design]
set_property CONFIG_VOLTAGE 2.5 [current_design]
set_property BITSTREAM.CONFIG.UNUSEDPIN PULLNONE [current_design]
set_property BITSTREAM.CONFIG.OVERTEMPPOWERDOWN ENABLE [current_design]

# set_property IODELAY_GROUP adc_iodelay_grp [get_cells system_i/adc_dac/selectio_adc_d*_*/inst/pins[0].idelaye2_bus]
# set_property IODELAY_GROUP adc_iodelay_grp [get_cells system_i/adc_dac/selectio_adc_d*_*/inst/pins[0].delayctrl]

set_property IODELAY_GROUP IO_DLY1 [get_cells *idelayctrl*]
set_property IODELAY_GROUP IO_DLY1 [get_cells -hier *idelaye2*]

# ----------------------------------------------------------------------------------
# RF ADC0 (Bank 34)
# ----------------------------------------------------------------------------------

# Clock output
set_property IOSTANDARD LVDS_25 [get_ports adc0_clk*]

set_property PACKAGE_PIN P20 [get_ports adc0_clk_out_n] ;# SRCC
set_property PACKAGE_PIN N20 [get_ports adc0_clk_out_p] ;# SRCC

# Data inputs
set_property IOSTANDARD LVDS_25 [get_ports adc0_d*]
set_property DIFF_TERM TRUE [get_ports adc0_d*]

set_property PACKAGE_PIN P18 [get_ports adc0_db_n]
set_property PACKAGE_PIN N17 [get_ports adc0_db_p]
set_property PACKAGE_PIN W20 [get_ports adc0_da_n]
set_property PACKAGE_PIN V20 [get_ports adc0_da_p]
set_property PACKAGE_PIN U20 [get_ports adc0_dco_n]
set_property PACKAGE_PIN T20 [get_ports adc0_dco_p]

# Control pins
set_property IOSTANDARD LVCMOS25 [get_ports adc0_ctl*]

set_property PACKAGE_PIN V15 [get_ports adc0_ctl_range_sel]
set_property PACKAGE_PIN Y19 [get_ports adc0_ctl_twolanes] ;# Controls also ADC1
set_property PACKAGE_PIN W18 [get_ports adc0_ctl_testpat]
set_property PACKAGE_PIN W19 [get_ports adc0_ctl_en]

# ----------------------------------------------------------------------------------
# RF ADC1 (Bank 34)
# ----------------------------------------------------------------------------------

# Clock output
set_property IOSTANDARD LVDS_25 [get_ports adc1_clk*]

set_property PACKAGE_PIN P19 [get_ports adc1_clk_out_n] ;# MRCC
set_property PACKAGE_PIN N18 [get_ports adc1_clk_out_p] ;# MRCC

# Data inputs
set_property IOSTANDARD LVDS_25 [get_ports adc1_d*]
set_property DIFF_TERM TRUE [get_ports adc1_d*]

set_property PACKAGE_PIN T15 [get_ports adc1_db_n]
set_property PACKAGE_PIN T14 [get_ports adc1_db_p]
set_property PACKAGE_PIN U17 [get_ports adc1_da_n]
set_property PACKAGE_PIN T16 [get_ports adc1_da_p]
set_property PACKAGE_PIN P16 [get_ports adc1_dco_n]
set_property PACKAGE_PIN P15 [get_ports adc1_dco_p]

# Control pins
set_property IOSTANDARD LVCMOS25 [get_ports adc1_ctl*]

set_property PACKAGE_PIN W15 [get_ports adc1_ctl_range_sel]
set_property PACKAGE_PIN Y17 [get_ports adc1_ctl_testpat]
set_property PACKAGE_PIN Y18 [get_ports adc1_ctl_en]

# ----------------------------------------------------------------------------------
# EXP_IO_B34 (Bank 34)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS25 [get_ports exp_io_b34_*]

set_property PACKAGE_PIN R14 [get_ports exp_io_b34_0_n]
set_property PACKAGE_PIN P14 [get_ports exp_io_b34_0_p]
set_property PACKAGE_PIN Y14 [get_ports exp_io_b34_1_n]
set_property PACKAGE_PIN W14 [get_ports exp_io_b34_1_p]
set_property PACKAGE_PIN W13 [get_ports exp_io_b34_2_n]
set_property PACKAGE_PIN V12 [get_ports exp_io_b34_2_p]
set_property PACKAGE_PIN T10 [get_ports exp_io_b34_3_n]
set_property PACKAGE_PIN T11 [get_ports exp_io_b34_3_p]
set_property PACKAGE_PIN V13 [get_ports exp_io_b34_4_n]
set_property PACKAGE_PIN U13 [get_ports exp_io_b34_4_p]

# ----------------------------------------------------------------------------------
# Clocks (Bank 34)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVDS_25 [get_ports clk_gen_in_clk_p]
set_property DIFF_TERM TRUE [get_ports clk_gen_in_clk_p]
set_property IOSTANDARD LVDS_25 [get_ports clk_gen_out_p]

set_property PACKAGE_PIN U19 [get_ports clk_gen_in_clk_n] ;# MRCC
set_property PACKAGE_PIN U18 [get_ports clk_gen_in_clk_p] ;# MRCC

set_property PACKAGE_PIN U15 [get_ports clk_gen_out_n] ;# SRCC
set_property PACKAGE_PIN U14 [get_ports clk_gen_out_p] ;# SRCC

# ----------------------------------------------------------------------------------
# Configuration SPI (Bank 34)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS25 [get_ports spi_cfg_*]

set_property PACKAGE_PIN R17 [get_ports spi_cfg_sdo]
set_property PACKAGE_PIN R16 [get_ports spi_cfg_sdi]
set_property PACKAGE_PIN W16 [get_ports spi_cfg_sck]

set_property PACKAGE_PIN U12 [get_ports spi_cfg_cs_rf_dac]
set_property PACKAGE_PIN T12 [get_ports spi_cfg_cs_clk_gen]

# ----------------------------------------------------------------------------------
# ADP5071 SYNC (Bank 34)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS25 [get_ports adp5071_sync]
set_property PACKAGE_PIN V16 [get_ports adp5071_sync]

# ----------------------------------------------------------------------------------
# Precision DAC (Bank 34)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS25 [get_ports spi_precision_dac_*]

set_property PACKAGE_PIN V17 [get_ports spi_precision_dac_cs]
set_property PACKAGE_PIN V18 [get_ports spi_precision_dac_sck]
set_property PACKAGE_PIN T17 [get_ports spi_precision_dac_sdi]
set_property PACKAGE_PIN R18 [get_ports spi_precision_dac_ldac]

# ----------------------------------------------------------------------------------
# EXP_IO_B35 (Bank 35)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS33 [get_ports exp_io_b35_*]

set_property PACKAGE_PIN J14 [get_ports exp_io_b35_0_n] ;# AD6N
set_property PACKAGE_PIN K14 [get_ports exp_io_b35_0_p] ;# AD6P
set_property PACKAGE_PIN L15 [get_ports exp_io_b35_1_n] ;# AD7N
set_property PACKAGE_PIN L14 [get_ports exp_io_b35_1_p] ;# AD7P
set_property PACKAGE_PIN M15 [get_ports exp_io_b35_2_n]
set_property PACKAGE_PIN M14 [get_ports exp_io_b35_2_p]
set_property PACKAGE_PIN J19 [get_ports exp_io_b35_3_n]
set_property PACKAGE_PIN K19 [get_ports exp_io_b35_3_p]
set_property PACKAGE_PIN L20 [get_ports exp_io_b35_4_n] ;# AD3N
set_property PACKAGE_PIN L19 [get_ports exp_io_b35_4_p] ;# AD3P
set_property PACKAGE_PIN H17 [get_ports exp_io_b35_5_n] ;# MRCC
set_property PACKAGE_PIN H16 [get_ports exp_io_b35_5_p] ;# MRCC
set_property PACKAGE_PIN M20 [get_ports exp_io_b35_6_n] ;# AD2N
set_property PACKAGE_PIN M19 [get_ports exp_io_b35_6_p] ;# AD2P
set_property PACKAGE_PIN N16 [get_ports exp_io_b35_7_n] ;# AD14N
set_property PACKAGE_PIN N15 [get_ports exp_io_b35_7_p] ;# AD14P

# ----------------------------------------------------------------------------------
# DAC (Bank 35)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS33 [get_ports dac_*]
set_property DRIVE 8 [get_ports dac_*]

# Channel 0
set_property PACKAGE_PIN D18 [get_ports {dac_0[0]}] ;# AD1N
set_property PACKAGE_PIN E17 [get_ports {dac_0[1]}] ;# AD1P
set_property PACKAGE_PIN E19 [get_ports {dac_0[2]}] ;# AD9N
set_property PACKAGE_PIN E18 [get_ports {dac_0[3]}] ;# AD9P
set_property PACKAGE_PIN A20 [get_ports {dac_0[4]}] ;# AD8N
set_property PACKAGE_PIN B19 [get_ports {dac_0[5]}] ;# AD8P
set_property PACKAGE_PIN F17 [get_ports {dac_0[6]}]
set_property PACKAGE_PIN F16 [get_ports {dac_0[7]}]
set_property PACKAGE_PIN B20 [get_ports {dac_0[8]}]
set_property PACKAGE_PIN C20 [get_ports {dac_0[9]}]
set_property PACKAGE_PIN L17 [get_ports {dac_0[10]}] ;# SRCC
set_property PACKAGE_PIN L16 [get_ports {dac_0[11]}] ;# SRCC
set_property PACKAGE_PIN D20 [get_ports {dac_0[12]}]
set_property PACKAGE_PIN D19 [get_ports {dac_0[13]}]
set_property PACKAGE_PIN G18 [get_ports {dac_0[14]}]
set_property PACKAGE_PIN G17 [get_ports {dac_0[15]}]

# Channel 1
set_property PACKAGE_PIN F20 [get_ports {dac_1[0]}] ;# AD12N
set_property PACKAGE_PIN F19 [get_ports {dac_1[1]}] ;# AD12P
set_property PACKAGE_PIN J16 [get_ports {dac_1[2]}] ;# AD15N
set_property PACKAGE_PIN K16 [get_ports {dac_1[3]}] ;# AD15P
set_property PACKAGE_PIN G20 [get_ports {dac_1[4]}] ;# AD13N
set_property PACKAGE_PIN G19 [get_ports {dac_1[5]}] ;# AD13P
set_property PACKAGE_PIN K18 [get_ports {dac_1[6]}] ;# MRCC
set_property PACKAGE_PIN K17 [get_ports {dac_1[7]}] ;# MRCC
set_property PACKAGE_PIN H20 [get_ports {dac_1[8]}] ;# AD5N
set_property PACKAGE_PIN J20 [get_ports {dac_1[9]}] ;# AD5P
set_property PACKAGE_PIN M18 [get_ports {dac_1[10]}] ;# AD10N
set_property PACKAGE_PIN M17 [get_ports {dac_1[11]}] ;# AD10P
set_property PACKAGE_PIN H18 [get_ports {dac_1[12]}] ;# SRCC, AD4N
set_property PACKAGE_PIN J18 [get_ports {dac_1[13]}] ;# SRCC, AD4P
set_property PACKAGE_PIN G15 [get_ports {dac_1[14]}]
set_property PACKAGE_PIN H15 [get_ports {dac_1[15]}]

# ----------------------------------------------------------------------------------
# XADC
# ----------------------------------------------------------------------------------

set_property PACKAGE_PIN K9  [get_ports Vp_Vn_v_p]
set_property PACKAGE_PIN L10 [get_ports Vp_Vn_v_n]
