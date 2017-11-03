# Koheron Alpha constraints file

set_property CFGBVS GND [current_design]
set_property CONFIG_VOLTAGE 1.8 [current_design]
set_property BITSTREAM.CONFIG.UNUSEDPIN PULLNONE [current_design]
set_property BITSTREAM.CONFIG.OVERTEMPPOWERDOWN ENABLE [current_design]

# ----------------------------------------------------------------------------------
# RF ADC (Bank 34)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD DIFF_HSTL_I_18 [get_ports adc_*]

set_property PACKAGE_PIN P19 [get_ports adc_clk_in_clk_n] ;# MRCC
set_property PACKAGE_PIN N18 [get_ports adc_clk_in_clk_p] ;# MRCC

# Channel A
set_property PACKAGE_PIN U17 [get_ports {adc_0_n[0]}]
set_property PACKAGE_PIN T16 [get_ports {adc_0_p[0]}]
set_property PACKAGE_PIN Y19 [get_ports {adc_0_n[1]}]
set_property PACKAGE_PIN Y18 [get_ports {adc_0_p[1]}]
set_property PACKAGE_PIN P16 [get_ports {adc_0_n[2]}]
set_property PACKAGE_PIN P15 [get_ports {adc_0_p[2]}]
set_property PACKAGE_PIN W19 [get_ports {adc_0_n[3]}]
set_property PACKAGE_PIN W18 [get_ports {adc_0_p[3]}]
set_property PACKAGE_PIN P18 [get_ports {adc_0_n[4]}]
set_property PACKAGE_PIN N17 [get_ports {adc_0_p[4]}]
set_property PACKAGE_PIN W20 [get_ports {adc_0_n[5]}]
set_property PACKAGE_PIN V20 [get_ports {adc_0_p[5]}]
set_property PACKAGE_PIN U20 [get_ports {adc_0_n[6]}]
set_property PACKAGE_PIN T20 [get_ports {adc_0_p[6]}]

# Channel B
set_property PACKAGE_PIN W13 [get_ports {adc_1_n[0]}]
set_property PACKAGE_PIN V12 [get_ports {adc_1_p[0]}]
set_property PACKAGE_PIN Y14 [get_ports {adc_1_n[1]}]
set_property PACKAGE_PIN W14 [get_ports {adc_1_p[1]}]
set_property PACKAGE_PIN P20 [get_ports {adc_1_n[2]}] ;# SRCC
set_property PACKAGE_PIN N20 [get_ports {adc_1_p[2]}] ;# SRCC
set_property PACKAGE_PIN R14 [get_ports {adc_1_n[3]}]
set_property PACKAGE_PIN P14 [get_ports {adc_1_p[3]}]
set_property PACKAGE_PIN W15 [get_ports {adc_1_n[4]}]
set_property PACKAGE_PIN V15 [get_ports {adc_1_p[4]}]
set_property PACKAGE_PIN T15 [get_ports {adc_1_n[5]}]
set_property PACKAGE_PIN T14 [get_ports {adc_1_p[5]}]
set_property PACKAGE_PIN Y17 [get_ports {adc_1_n[6]}]
set_property PACKAGE_PIN Y16 [get_ports {adc_1_p[6]}]

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

set_property PACKAGE_PIN V16 [get_ports spi_cfg_cs_rf_adc]
set_property PACKAGE_PIN U12 [get_ports spi_cfg_cs_rf_dac]
set_property PACKAGE_PIN T12 [get_ports spi_cfg_cs_clk_gen]

# ----------------------------------------------------------------------------------
# Slow DAC (Bank 34)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS18 [get_ports spi_slow_dac_*]

set_property PACKAGE_PIN V17 [get_ports spi_slow_dac_cs]
set_property PACKAGE_PIN V18 [get_ports spi_slow_dac_sck]
set_property PACKAGE_PIN T17 [get_ports spi_slow_dac_sdi]
set_property PACKAGE_PIN R18 [get_ports spi_slow_dac_ldac]

# ----------------------------------------------------------------------------------
# Slow ADC (Bank 34)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS18 [get_ports spi_slow_adc_*]

set_property PACKAGE_PIN U13 [get_ports spi_slow_adc_cs]
set_property PACKAGE_PIN V13 [get_ports spi_slow_adc_sck]
set_property PACKAGE_PIN T11 [get_ports spi_slow_adc_sdi]
set_property PACKAGE_PIN T10 [get_ports spi_slow_adc_sdo]


# ----------------------------------------------------------------------------------
# Expansion connector IOs (Bank 35)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS33 [get_ports exp_io_*]

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
# DAC (Bank 35)
# ----------------------------------------------------------------------------------

set_property IOSTANDARD LVCMOS33 [get_ports dac_*]
set_property DRIVE 8 [get_ports dac_*]

# Channel 1
set_property PACKAGE_PIN F20 [get_ports {dac_0[0]}] ;# AD12N
set_property PACKAGE_PIN F19 [get_ports {dac_0[1]}] ;# AD12P
set_property PACKAGE_PIN J16 [get_ports {dac_0[2]}] ;# AD15N
set_property PACKAGE_PIN K16 [get_ports {dac_0[3]}] ;# AD15P
set_property PACKAGE_PIN G20 [get_ports {dac_0[4]}] ;# AD13N
set_property PACKAGE_PIN G19 [get_ports {dac_0[5]}] ;# AD13P
set_property PACKAGE_PIN K18 [get_ports {dac_0[6]}] ;# MRCC
set_property PACKAGE_PIN K17 [get_ports {dac_0[7]}] ;# MRCC
set_property PACKAGE_PIN H20 [get_ports {dac_0[8]}] ;# AD5N
set_property PACKAGE_PIN J20 [get_ports {dac_0[9]}] ;# AD5P
set_property PACKAGE_PIN M18 [get_ports {dac_0[10]}] ;# AD10N
set_property PACKAGE_PIN M17 [get_ports {dac_0[11]}] ;# AD10P
set_property PACKAGE_PIN H18 [get_ports {dac_0[12]}] ;# SRCC, AD4N
set_property PACKAGE_PIN J18 [get_ports {dac_0[13]}] ;# SRCC, AD4P
set_property PACKAGE_PIN G15 [get_ports {dac_0[14]}]
set_property PACKAGE_PIN H15 [get_ports {dac_0[15]}]

# Channel 2
set_property PACKAGE_PIN D18 [get_ports {dac_1[0]}] ;# AD1N
set_property PACKAGE_PIN E17 [get_ports {dac_1[1]}] ;# AD1P
set_property PACKAGE_PIN E19 [get_ports {dac_1[2]}] ;# AD9N
set_property PACKAGE_PIN E18 [get_ports {dac_1[3]}] ;# AD9P
set_property PACKAGE_PIN A20 [get_ports {dac_1[4]}] ;# AD8N
set_property PACKAGE_PIN B19 [get_ports {dac_1[5]}] ;# AD8P
set_property PACKAGE_PIN F17 [get_ports {dac_1[6]}]
set_property PACKAGE_PIN F16 [get_ports {dac_1[7]}]
set_property PACKAGE_PIN B20 [get_ports {dac_1[8]}]
set_property PACKAGE_PIN C20 [get_ports {dac_1[9]}]
set_property PACKAGE_PIN L17 [get_ports {dac_1[10]}] ;# SRCC
set_property PACKAGE_PIN L16 [get_ports {dac_1[11]}] ;# SRCC
set_property PACKAGE_PIN D20 [get_ports {dac_1[12]}]
set_property PACKAGE_PIN D19 [get_ports {dac_1[13]}]
set_property PACKAGE_PIN G18 [get_ports {dac_1[14]}]
set_property PACKAGE_PIN G17 [get_ports {dac_1[15]}]

# ----------------------------------------------------------------------------------
# Bank 13 connected with MIO in parallel (set to high-Z if not used)
# ----------------------------------------------------------------------------------

# set_property PACKAGE_PIN Y6 [get_ports I2C_INT#_R] ;# MIO9 // B13
# set_property PACKAGE_PIN V8 [get_ports I2C_SCL] ;# MIO14 // B13
# set_property PACKAGE_PIN W8 [get_ports I2C_SDA] ;# MIO15 // B13
# set_property PACKAGE_PIN W9 [get_ports SDCMD] ;# MIO40 // B13
# set_property PACKAGE_PIN W10 [get_ports SDCLK] ;# MIO41 // B13
# set_property PACKAGE_PIN W11 [get_ports SDD0] ;# MIO42 // B13
# set_property PACKAGE_PIN Y11 [get_ports SDD1] ;# MIO43 // B13
# set_property PACKAGE_PIN Y9 [get_ports SDD2] ;# MIO44 // B13
# set_property PACKAGE_PIN Y8 [get_ports SDD3] ;# MIO45 // B13
# set_property PACKAGE_PIN T5 [get_ports UART_RX] ;# MIO46 // B13
# set_property PACKAGE_PIN U5 [get_ports UART_TX] ;# MIO47 // B13
# set_property PACKAGE_PIN U7 [get_ports I2C1_SCL_LS] ;# MIO48 // B13 SRCC
# set_property PACKAGE_PIN V7 [get_ports I2C1_SDA_LS] ;# MIO49 // B13 SRCC
# set_property PACKAGE_PIN T9 [get_ports SDCD#] ;# MIO50 // B13 MRCC
# set_property PACKAGE_PIN U10 [get_ports I2C1_INT#R] ;# MIO51 // B13 MRCC

# set_property PACKAGE_PIN V5 [get_ports ETH_LED]
# set_property IOSTANDARD LVCMOS18 [get_ports ETH_LED]

# 33.333 MHZ clock
# set_property PACKAGE_PIN Y7 [get_ports CLK33] ;# MRCC
# set_property IOSTANDARD LVCMOS18 [get_ports CLK33]

# ----------------------------------------------------------------------------------
# LEDs
# ----------------------------------------------------------------------------------

#set_property PACKAGE_PIN R19 [get_ports LED0#_PL] ;# IO_0_34
#set_property PACKAGE_PIN T19 [get_ports LED1#_PL] ;# IO_25_34
#set_property PACKAGE_PIN G14 [get_ports LED2#_PL] ;# IO_0_35
#set_property PACKAGE_PIN J15 [get_ports LED3#_PL] ;# IO_25_35

#set_property IOSTANDARD LVCMOS18 [get_ports LED0#_PL]
#set_property IOSTANDARD LVCMOS18 [get_ports LED1#_PL]
#set_property IOSTANDARD LVCMOS33 [get_ports LED2#_PL]
#set_property IOSTANDARD LVCMOS33 [get_ports LED3#_PL]

# ----------------------------------------------------------------------------------
# XADC
# ----------------------------------------------------------------------------------

set_property PACKAGE_PIN K9  [get_ports Vp_Vn_v_p]
set_property PACKAGE_PIN L10 [get_ports Vp_Vn_v_n]