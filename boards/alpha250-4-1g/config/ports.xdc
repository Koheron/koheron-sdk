# Koheron ALPHA250-4 constraints file

set_property CFGBVS GND [current_design]
set_property CONFIG_VOLTAGE 1.8 [current_design]
set_property BITSTREAM.CONFIG.UNUSEDPIN PULLNONE [current_design]
set_property BITSTREAM.CONFIG.OVERTEMPPOWERDOWN ENABLE [current_design]

# ----------------------------------------------------------------------------------
# RF ADC 1 (Bank 34)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD DIFF_HSTL_I_18 [get_ports adc1_*]

set_property PACKAGE_PIN M20 [get_ports adc1_clk_in_clk_n] ;# MRCC
set_property PACKAGE_PIN M19 [get_ports adc1_clk_in_clk_p] ;# MRCC

# Channel 0
set_property PACKAGE_PIN K21 [get_ports {adc1_0_n[0]}]
set_property PACKAGE_PIN J20 [get_ports {adc1_0_p[0]}]
set_property PACKAGE_PIN L22 [get_ports {adc1_0_n[1]}]
set_property PACKAGE_PIN L21 [get_ports {adc1_0_p[1]}]
set_property PACKAGE_PIN J17 [get_ports {adc1_0_n[2]}]
set_property PACKAGE_PIN J16 [get_ports {adc1_0_p[2]}]
set_property PACKAGE_PIN J22 [get_ports {adc1_0_n[3]}]
set_property PACKAGE_PIN J21 [get_ports {adc1_0_p[3]}]
set_property PACKAGE_PIN L16 [get_ports {adc1_0_n[4]}]
set_property PACKAGE_PIN K16 [get_ports {adc1_0_p[4]}]
set_property PACKAGE_PIN K18 [get_ports {adc1_0_n[5]}]
set_property PACKAGE_PIN J18 [get_ports {adc1_0_p[5]}]
set_property PACKAGE_PIN K15 [get_ports {adc1_0_n[6]}]
set_property PACKAGE_PIN J15 [get_ports {adc1_0_p[6]}]

# Channel 1
set_property PACKAGE_PIN N18 [get_ports {adc1_1_n[0]}]
set_property PACKAGE_PIN N17 [get_ports {adc1_1_p[0]}]
set_property PACKAGE_PIN P22 [get_ports {adc1_1_n[1]}]
set_property PACKAGE_PIN N22 [get_ports {adc1_1_p[1]}]
set_property PACKAGE_PIN N20 [get_ports {adc1_1_n[2]}] ;# SRCC
set_property PACKAGE_PIN N19 [get_ports {adc1_1_p[2]}] ;# SRCC
set_property PACKAGE_PIN M16 [get_ports {adc1_1_n[3]}]
set_property PACKAGE_PIN M15 [get_ports {adc1_1_p[3]}]
set_property PACKAGE_PIN R21 [get_ports {adc1_1_n[4]}]
set_property PACKAGE_PIN R20 [get_ports {adc1_1_p[4]}]
set_property PACKAGE_PIN M17 [get_ports {adc1_1_n[5]}]
set_property PACKAGE_PIN L17 [get_ports {adc1_1_p[5]}]
set_property PACKAGE_PIN M22 [get_ports {adc1_1_n[6]}]
set_property PACKAGE_PIN M21 [get_ports {adc1_1_p[6]}]

# ----------------------------------------------------------------------------------
# Clocks (Bank 34)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD DIFF_HSTL_I_18 [get_ports clk_gen_in_clk_p]
set_property IOSTANDARD DIFF_HSTL_I_18 [get_ports clk_gen_out_p]

set_property PACKAGE_PIN L19 [get_ports clk_gen_in_clk_n] ;# MRCC
set_property PACKAGE_PIN L18 [get_ports clk_gen_in_clk_p] ;# MRCC

set_property PACKAGE_PIN K20 [get_ports clk_gen_out_n] ;# SRCC
set_property PACKAGE_PIN K19 [get_ports clk_gen_out_p] ;# SRCC

# ----------------------------------------------------------------------------------
# Configuration SPI (Bank 34)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS18 [get_ports spi_cfg_*]

set_property PACKAGE_PIN P15 [get_ports spi_cfg_sdo]
set_property PACKAGE_PIN N15 [get_ports spi_cfg_sdi]
set_property PACKAGE_PIN P21 [get_ports spi_cfg_sck]

set_property PACKAGE_PIN R16 [get_ports spi_cfg_cs_rf_adc0]
set_property PACKAGE_PIN P20 [get_ports spi_cfg_cs_rf_adc1]
set_property PACKAGE_PIN P16 [get_ports spi_cfg_cs_clk_gen]

# ----------------------------------------------------------------------------------
# I2C0 (Bank 34)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS18 [get_ports iic_0_*]

set_property PACKAGE_PIN H15 [get_ports iic_0_sda_io]
set_property PACKAGE_PIN R15 [get_ports iic_0_scl_io]

# ----------------------------------------------------------------------------------
# Precision DAC (Bank 34)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS18 [get_ports spi_precision_dac_*]

set_property PACKAGE_PIN T16 [get_ports spi_precision_dac_cs]
set_property PACKAGE_PIN T17 [get_ports spi_precision_dac_sck]
set_property PACKAGE_PIN P17 [get_ports spi_precision_dac_sdi]
set_property PACKAGE_PIN P18 [get_ports spi_precision_dac_ldac]

# ----------------------------------------------------------------------------------
# Precision ADC (Bank 34)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS18 [get_ports spi_precision_adc_*]

set_property PACKAGE_PIN R18 [get_ports spi_precision_adc_cs]
set_property PACKAGE_PIN T18 [get_ports spi_precision_adc_sck]
set_property PACKAGE_PIN R19 [get_ports spi_precision_adc_sdi]
set_property PACKAGE_PIN T19 [get_ports spi_precision_adc_sdo]


# ----------------------------------------------------------------------------------
# Expansion connector IOs (Bank 35)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS33 [get_ports exp_io_*]

set_property PACKAGE_PIN F19 [get_ports exp_io_0_n] ;# AD6N
set_property PACKAGE_PIN G19 [get_ports exp_io_0_p] ;# AD6P
set_property PACKAGE_PIN G21 [get_ports exp_io_1_n] ;# AD7N
set_property PACKAGE_PIN G20 [get_ports exp_io_1_p] ;# AD7P
set_property PACKAGE_PIN H20 [get_ports exp_io_2_n]
set_property PACKAGE_PIN H19 [get_ports exp_io_2_p]
set_property PACKAGE_PIN D21 [get_ports exp_io_3_n]
set_property PACKAGE_PIN E21 [get_ports exp_io_3_p]
set_property PACKAGE_PIN F22 [get_ports exp_io_4_n] ;# AD3N
set_property PACKAGE_PIN F21 [get_ports exp_io_4_p] ;# AD3P
set_property PACKAGE_PIN B20 [get_ports exp_io_5_n] ;# MRCC
set_property PACKAGE_PIN B19 [get_ports exp_io_5_p] ;# MRCC
set_property PACKAGE_PIN G22 [get_ports exp_io_6_n] ;# AD2N
set_property PACKAGE_PIN H22 [get_ports exp_io_6_p] ;# AD2P
set_property PACKAGE_PIN E20 [get_ports exp_io_7_n] ;# AD14N
set_property PACKAGE_PIN E19 [get_ports exp_io_7_p] ;# AD14P

# ----------------------------------------------------------------------------------
# RF ADC 0 (Bank 35)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD DIFF_HSTL_I_18 [get_ports adc0_*]

set_property PACKAGE_PIN C19 [get_ports {adc0_clk_in_clk_n}] ;# MRCC
set_property PACKAGE_PIN D18 [get_ports {adc0_clk_in_clk_p}] ;# MRCC

# Channel 0
set_property PACKAGE_PIN A22 [get_ports {adc0_0_n[0]}]
set_property PACKAGE_PIN A21 [get_ports {adc0_0_p[0]}]
set_property PACKAGE_PIN C18 [get_ports {adc0_0_n[1]}]
set_property PACKAGE_PIN C17 [get_ports {adc0_0_p[1]}]
set_property PACKAGE_PIN E18 [get_ports {adc0_0_n[2]}]
set_property PACKAGE_PIN F18 [get_ports {adc0_0_p[2]}]
set_property PACKAGE_PIN B15 [get_ports {adc0_0_n[3]}]
set_property PACKAGE_PIN C15 [get_ports {adc0_0_p[3]}]
set_property PACKAGE_PIN D15 [get_ports {adc0_0_n[4]}]
set_property PACKAGE_PIN E15 [get_ports {adc0_0_p[4]}]
set_property PACKAGE_PIN B17 [get_ports {adc0_0_n[5]}]
set_property PACKAGE_PIN B16 [get_ports {adc0_0_p[5]}]
set_property PACKAGE_PIN G16 [get_ports {adc0_0_n[6]}]
set_property PACKAGE_PIN G15 [get_ports {adc0_0_p[6]}]

# Channel 1
set_property PACKAGE_PIN F17 [get_ports {adc0_1_n[0]}]
set_property PACKAGE_PIN G17 [get_ports {adc0_1_p[0]}]
set_property PACKAGE_PIN E16 [get_ports {adc0_1_n[1]}]
set_property PACKAGE_PIN F16 [get_ports {adc0_1_p[1]}]
set_property PACKAGE_PIN C20 [get_ports {adc0_1_n[2]}]
set_property PACKAGE_PIN D20 [get_ports {adc0_1_p[2]}]
set_property PACKAGE_PIN C22 [get_ports {adc0_1_n[3]}]
set_property PACKAGE_PIN D22 [get_ports {adc0_1_p[3]}]
set_property PACKAGE_PIN D17 [get_ports {adc0_1_n[4]}]
set_property PACKAGE_PIN D16 [get_ports {adc0_1_p[4]}]
set_property PACKAGE_PIN A19 [get_ports {adc0_1_n[5]}]
set_property PACKAGE_PIN A18 [get_ports {adc0_1_p[5]}]
set_property PACKAGE_PIN B22 [get_ports {adc0_1_n[6]}]
set_property PACKAGE_PIN B21 [get_ports {adc0_1_p[6]}]

# ----------------------------------------------------------------------------------
# XADC
# ----------------------------------------------------------------------------------

set_property PACKAGE_PIN K9  [get_ports Vp_Vn_v_p]
set_property PACKAGE_PIN L10 [get_ports Vp_Vn_v_n]
