# Koheron ALPHA250-4 constraints file

set_property CFGBVS GND [current_design]
set_property CONFIG_VOLTAGE 1.8 [current_design]
set_property BITSTREAM.CONFIG.UNUSEDPIN PULLNONE [current_design]
set_property BITSTREAM.CONFIG.OVERTEMPPOWERDOWN ENABLE [current_design]

# ----------------------------------------------------------------------------------
# RF ADC 1 (Bank 34)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD DIFF_HSTL_I_18 [get_ports adc1_*]

set_property PACKAGE_PIN P19 [get_ports adc1_clk_in_clk_n] ;# MRCC
set_property PACKAGE_PIN N18 [get_ports adc1_clk_in_clk_p] ;# MRCC

# Channel 0
set_property PACKAGE_PIN U17 [get_ports {adc1_0_n[0]}]
set_property PACKAGE_PIN T16 [get_ports {adc1_0_p[0]}]
set_property PACKAGE_PIN Y19 [get_ports {adc1_0_n[1]}]
set_property PACKAGE_PIN Y18 [get_ports {adc1_0_p[1]}]
set_property PACKAGE_PIN P16 [get_ports {adc1_0_n[2]}]
set_property PACKAGE_PIN P15 [get_ports {adc1_0_p[2]}]
set_property PACKAGE_PIN W19 [get_ports {adc1_0_n[3]}]
set_property PACKAGE_PIN W18 [get_ports {adc1_0_p[3]}]
set_property PACKAGE_PIN P18 [get_ports {adc1_0_n[4]}]
set_property PACKAGE_PIN N17 [get_ports {adc1_0_p[4]}]
set_property PACKAGE_PIN W20 [get_ports {adc1_0_n[5]}]
set_property PACKAGE_PIN V20 [get_ports {adc1_0_p[5]}]
set_property PACKAGE_PIN U20 [get_ports {adc1_0_n[6]}]
set_property PACKAGE_PIN T20 [get_ports {adc1_0_p[6]}]

# Channel 1
set_property PACKAGE_PIN W13 [get_ports {adc1_1_n[0]}]
set_property PACKAGE_PIN V12 [get_ports {adc1_1_p[0]}]
set_property PACKAGE_PIN Y14 [get_ports {adc1_1_n[1]}]
set_property PACKAGE_PIN W14 [get_ports {adc1_1_p[1]}]
set_property PACKAGE_PIN P20 [get_ports {adc1_1_n[2]}] ;# SRCC
set_property PACKAGE_PIN N20 [get_ports {adc1_1_p[2]}] ;# SRCC
set_property PACKAGE_PIN R14 [get_ports {adc1_1_n[3]}]
set_property PACKAGE_PIN P14 [get_ports {adc1_1_p[3]}]
set_property PACKAGE_PIN W15 [get_ports {adc1_1_n[4]}]
set_property PACKAGE_PIN V15 [get_ports {adc1_1_p[4]}]
set_property PACKAGE_PIN T15 [get_ports {adc1_1_n[5]}]
set_property PACKAGE_PIN T14 [get_ports {adc1_1_p[5]}]
set_property PACKAGE_PIN Y17 [get_ports {adc1_1_n[6]}]
set_property PACKAGE_PIN Y16 [get_ports {adc1_1_p[6]}]

# ----------------------------------------------------------------------------------
# Clocks (Bank 34)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD DIFF_HSTL_I_18 [get_ports clk_gen_in_clk_p]
set_property IOSTANDARD DIFF_HSTL_I_18 [get_ports clk_gen_out_p]

set_property PACKAGE_PIN U19 [get_ports clk_gen_in_clk_n] ;# MRCC
set_property PACKAGE_PIN U18 [get_ports clk_gen_in_clk_p] ;# MRCC

set_property PACKAGE_PIN U15 [get_ports clk_gen_out_n] ;# SRCC
set_property PACKAGE_PIN U14 [get_ports clk_gen_out_p] ;# SRCC

# ----------------------------------------------------------------------------------
# Configuration SPI (Bank 34)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS18 [get_ports spi_cfg_*]

set_property PACKAGE_PIN R17 [get_ports spi_cfg_sdo]
set_property PACKAGE_PIN R16 [get_ports spi_cfg_sdi]
set_property PACKAGE_PIN W16 [get_ports spi_cfg_sck]

set_property PACKAGE_PIN U12 [get_ports spi_cfg_cs_rf_adc0]
set_property PACKAGE_PIN V16 [get_ports spi_cfg_cs_rf_adc1]
set_property PACKAGE_PIN T12 [get_ports spi_cfg_cs_clk_gen]

# ----------------------------------------------------------------------------------
# Precision DAC (Bank 34)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS18 [get_ports spi_precision_dac_*]

set_property PACKAGE_PIN V17 [get_ports spi_precision_dac_cs]
set_property PACKAGE_PIN V18 [get_ports spi_precision_dac_sck]
set_property PACKAGE_PIN T17 [get_ports spi_precision_dac_sdi]
set_property PACKAGE_PIN R18 [get_ports spi_precision_dac_ldac]

# ----------------------------------------------------------------------------------
# Precision ADC (Bank 34)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS18 [get_ports spi_precision_adc_*]

set_property PACKAGE_PIN U13 [get_ports spi_precision_adc_cs]
set_property PACKAGE_PIN V13 [get_ports spi_precision_adc_sck]
set_property PACKAGE_PIN T11 [get_ports spi_precision_adc_sdi]
set_property PACKAGE_PIN T10 [get_ports spi_precision_adc_sdo]


# ----------------------------------------------------------------------------------
# Expansion connector IOs (Bank 35)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS18 [get_ports exp_io_*]

set_property PACKAGE_PIN J14 [get_ports exp_io_0_n] ;# AD6N
set_property PACKAGE_PIN K14 [get_ports exp_io_0_p] ;# AD6P
set_property PACKAGE_PIN L15 [get_ports exp_io_1_n] ;# AD7N
set_property PACKAGE_PIN L14 [get_ports exp_io_1_p] ;# AD7P
set_property PACKAGE_PIN M15 [get_ports exp_io_2_n]
set_property PACKAGE_PIN M14 [get_ports exp_io_2_p]
set_property PACKAGE_PIN J19 [get_ports exp_io_3_n]
set_property PACKAGE_PIN K19 [get_ports exp_io_3_p]
set_property PACKAGE_PIN L20 [get_ports exp_io_4_n] ;# AD3N
set_property PACKAGE_PIN L19 [get_ports exp_io_4_p] ;# AD3P
set_property PACKAGE_PIN H17 [get_ports exp_io_5_n] ;# MRCC
set_property PACKAGE_PIN H16 [get_ports exp_io_5_p] ;# MRCC
set_property PACKAGE_PIN M20 [get_ports exp_io_6_n] ;# AD2N
set_property PACKAGE_PIN M19 [get_ports exp_io_6_p] ;# AD2P
set_property PACKAGE_PIN N16 [get_ports exp_io_7_n] ;# AD14N
set_property PACKAGE_PIN N15 [get_ports exp_io_7_p] ;# AD14P

# ----------------------------------------------------------------------------------
# RF ADC 0 (Bank 35)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD DIFF_HSTL_I_18 [get_ports adc0_*]

set_property PACKAGE_PIN K18 [get_ports {adc0_clk_in_clk_n}] ;# MRCC
set_property PACKAGE_PIN K17 [get_ports {adc0_clk_in_clk_p}] ;# MRCC

# Channel 0
set_property PACKAGE_PIN F20 [get_ports {adc0_0_n[0]}]
set_property PACKAGE_PIN F19 [get_ports {adc0_0_p[0]}]
set_property PACKAGE_PIN L17 [get_ports {adc0_0_n[1]}]
set_property PACKAGE_PIN L16 [get_ports {adc0_0_p[1]}]
set_property PACKAGE_PIN D20 [get_ports {adc0_0_n[2]}]
set_property PACKAGE_PIN D19 [get_ports {adc0_0_p[2]}]
set_property PACKAGE_PIN B20 [get_ports {adc0_0_n[3]}]
set_property PACKAGE_PIN C20 [get_ports {adc0_0_p[3]}]
set_property PACKAGE_PIN F17 [get_ports {adc0_0_n[4]}]
set_property PACKAGE_PIN F16 [get_ports {adc0_0_p[4]}]
set_property PACKAGE_PIN A20 [get_ports {adc0_0_n[5]}]
set_property PACKAGE_PIN B19 [get_ports {adc0_0_p[5]}]
set_property PACKAGE_PIN E19 [get_ports {adc0_0_n[6]}]
set_property PACKAGE_PIN E18 [get_ports {adc0_0_p[6]}]

# Channel 1
set_property PACKAGE_PIN G15 [get_ports {adc0_1_n[0]}]
set_property PACKAGE_PIN H15 [get_ports {adc0_1_p[0]}]
set_property PACKAGE_PIN M18 [get_ports {adc0_1_n[1]}]
set_property PACKAGE_PIN M17 [get_ports {adc0_1_p[1]}]
set_property PACKAGE_PIN H18 [get_ports {adc0_1_n[2]}]
set_property PACKAGE_PIN J18 [get_ports {adc0_1_p[2]}]
set_property PACKAGE_PIN H20 [get_ports {adc0_1_n[3]}]
set_property PACKAGE_PIN J20 [get_ports {adc0_1_p[3]}]
set_property PACKAGE_PIN J16 [get_ports {adc0_1_n[4]}]
set_property PACKAGE_PIN K16 [get_ports {adc0_1_p[4]}]
set_property PACKAGE_PIN G18 [get_ports {adc0_1_n[5]}]
set_property PACKAGE_PIN G17 [get_ports {adc0_1_p[5]}]
set_property PACKAGE_PIN G20 [get_ports {adc0_1_n[6]}]
set_property PACKAGE_PIN G19 [get_ports {adc0_1_p[6]}]

# ----------------------------------------------------------------------------------
# XADC
# ----------------------------------------------------------------------------------

set_property PACKAGE_PIN K9  [get_ports Vp_Vn_v_p]
set_property PACKAGE_PIN L10 [get_ports Vp_Vn_v_n]
