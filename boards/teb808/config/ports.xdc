

#System Controller IP
  #LED_HD SC0 J3:31
  #LED_XMOD SC17 J3:48 
  #CAN RX SC19 J3:52 B47_L2_P in
  #CAN TX SC18 J3:50 B47_L2_N out 
  #CAN S  SC16 J3:46 B47_L3_N out

set_property PACKAGE_PIN J14 [get_ports BASE_sc0]
set_property PACKAGE_PIN G13 [get_ports BASE_sc5]
set_property PACKAGE_PIN J15 [get_ports BASE_sc6]
set_property PACKAGE_PIN K15 [get_ports BASE_sc7]
set_property PACKAGE_PIN A15 [get_ports BASE_sc10_io]
set_property PACKAGE_PIN B15 [get_ports BASE_sc11]
set_property PACKAGE_PIN C13 [get_ports BASE_sc12]
set_property PACKAGE_PIN C14 [get_ports BASE_sc13]
set_property PACKAGE_PIN E13 [get_ports BASE_sc14]
set_property PACKAGE_PIN E14 [get_ports BASE_sc15]
set_property PACKAGE_PIN A13 [get_ports BASE_sc16]
set_property PACKAGE_PIN B13 [get_ports BASE_sc17]
set_property PACKAGE_PIN A14 [get_ports BASE_sc18]
set_property PACKAGE_PIN B14 [get_ports BASE_sc19]
set_property IOSTANDARD LVCMOS18 [get_ports BASE_sc0]
set_property IOSTANDARD LVCMOS18 [get_ports BASE_sc5]
set_property IOSTANDARD LVCMOS18 [get_ports BASE_sc6]
set_property IOSTANDARD LVCMOS18 [get_ports BASE_sc7]
set_property IOSTANDARD LVCMOS18 [get_ports BASE_sc10_io]
set_property IOSTANDARD LVCMOS18 [get_ports BASE_sc11]
set_property IOSTANDARD LVCMOS18 [get_ports BASE_sc12]
set_property IOSTANDARD LVCMOS18 [get_ports BASE_sc13]
set_property IOSTANDARD LVCMOS18 [get_ports BASE_sc14]
set_property IOSTANDARD LVCMOS18 [get_ports BASE_sc15]
set_property IOSTANDARD LVCMOS18 [get_ports BASE_sc16]
set_property IOSTANDARD LVCMOS18 [get_ports BASE_sc17]
set_property IOSTANDARD LVCMOS18 [get_ports BASE_sc18]
set_property IOSTANDARD LVCMOS18 [get_ports BASE_sc19]



# PLL
#  H6 [get_ports {si570_clk_p[0]}]
#set_property IOSTANDARD LVDS [get_ports {si570_clk_p[0]}]
#set_property IOSTANDARD LVDS [get_ports {si570_clk_n[0]}]

# Clocks
#set_property PACKAGE_PIN J8 [get_ports {B229_CLK1_clk_p[0]}]; #MGT clock   controlled from Si5345 (on SoM), out2
#set_property PACKAGE_PIN F25 [get_ports {B128_CLK0_clk_p[0]}]; #MGT clock  controlled from Si5345 (on SoM), out9
#set_property PACKAGE_PIN N8 [get_ports {B228_CLK1_clk_p[0]}]; #MGT clock  controlled from Si5345 (on SoM), out3
#

# FireFly
#set_property PACKAGE_PIN F25 [get_ports {FireFly_clk_p[0]}]; #MGT B128_CLK0 clock  controlled from Si5345 (on SoM), out9
set_property PACKAGE_PIN B29 [get_ports {FF_RX_p[0]}]; # B128_RX3_N
set_property PACKAGE_PIN B30 [get_ports {FF_RX_n[0]}]; # B128_RX3_N
set_property PACKAGE_PIN F29 [get_ports {FF_RX_p[1]}]; # B230_RX1_P
set_property PACKAGE_PIN F30 [get_ports {FF_RX_n[1]}]; # B230_RX1_P
set_property PACKAGE_PIN D29 [get_ports {FF_RX_p[2]}]; # B128_RX2_N
set_property PACKAGE_PIN D30 [get_ports {FF_RX_n[2]}]; # B128_RX2_N
set_property PACKAGE_PIN H29 [get_ports {FF_RX_p[3]}]; # B230_RX0_P
set_property PACKAGE_PIN H30 [get_ports {FF_RX_n[3]}]; # B230_RX0_P

set_property PACKAGE_PIN E27 [get_ports {FF_TX_p[0]}]; # B128_TX1_N
set_property PACKAGE_PIN E28 [get_ports {FF_TX_n[0]}]; # B128_TX1_N
set_property PACKAGE_PIN G27 [get_ports {FF_TX_p[1]}]; # B230_TX0_P
set_property PACKAGE_PIN G28 [get_ports {FF_TX_n[1]}]; # B230_TX0_P
set_property PACKAGE_PIN C27 [get_ports {FF_TX_p[2]}]; # B128_TX2_N
set_property PACKAGE_PIN C28 [get_ports {FF_TX_n[2]}]; # B128_TX2_N
set_property PACKAGE_PIN A27 [get_ports {FF_TX_p[3]}]; # B230_TX3_P
set_property PACKAGE_PIN A28 [get_ports {FF_TX_n[3]}]; # B230_TX3_P

# SFP 
set_property PACKAGE_PIN G8 [get_ports {SFP_clk_p}]; # B230_CLK0_clk_p controlled from Si5345 (on SoM), out1
set_property PACKAGE_PIN A4 [get_ports {SFP_RX_p[0]}]; # B230_RX3_P
set_property PACKAGE_PIN A8 [get_ports {SFP_TX_p[0]}]; # B230_TX3_P
set_property PACKAGE_PIN B2 [get_ports {SFP_RX_p[1]}]; # B230_RX2_P
set_property PACKAGE_PIN B6 [get_ports {SFP_TX_p[1]}]; # B230_TX2_P



# Audio Codec

#LRCLK		  J3:49 B47_L9_N
#BCLK		    J3:51 B47_L9_P
#DAC_SDATA	J3:53 B47_L7_N
#ADC_SDATA	J3:55 B47_L7_P

set_property PACKAGE_PIN G14 [get_ports I2S_lrclk ]
set_property PACKAGE_PIN G15 [get_ports I2S_bclk ]
set_property PACKAGE_PIN E15 [get_ports I2S_sdin ]
set_property PACKAGE_PIN F15 [get_ports I2S_sdout ]
set_property IOSTANDARD LVCMOS18 [get_ports I2S_lrclk ]
set_property IOSTANDARD LVCMOS18 [get_ports I2S_bclk ]
set_property IOSTANDARD LVCMOS18 [get_ports I2S_sdin ]
set_property IOSTANDARD LVCMOS18 [get_ports I2S_sdout ]






set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
set_property BITSTREAM.CONFIG.UNUSEDPIN PULLNONE [current_design]

