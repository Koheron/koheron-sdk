# MGT CONNECTIONS FMC
#
set_property PACKAGE_PIN R8 [get_ports GBTCLK_M2C_P[0]]; # B228_CLK0
set_property PACKAGE_PIN R7 [get_ports GBTCLK_M2C_N[0]]; 
set_property PACKAGE_PIN L8 [get_ports GBTCLK_M2C_P[1]]; # B229_CLK0
set_property PACKAGE_PIN L7 [get_ports GBTCLK_M2C_N[1]]; # B229_CLK0
set_property PACKAGE_PIN G8 [get_ports {B230_clk_p}] # B230_CLK0_clk_p controlled from Si5345 (on SoM), out1

set_property PACKAGE_PIN AH6 [get_ports CLK_M2C_P[0]]
set_property PACKAGE_PIN AJ6 [get_ports CLK_M2C_N[0]]
set_property PACKAGE_PIN E10 [get_ports CLK_M2C_P[1]]
set_property PACKAGE_PIN D10 [get_ports CLK_M2C_N[1]]
#set_property PACKAGE_PIN E10 [get_ports CLK_M2C_P[2]]; # configured as out from SI5338 CLK2
#set_property PACKAGE_PIN D10 [get_ports CLK_M2C_N[2]]; # configured as out from SI5338 CLK2
#set_property PACKAGE_PIN E10 [get_ports CLK_M2C_P[3]]; # configured as out from SI5338 CLK3
#set_property PACKAGE_PIN D10 [get_ports CLK_M2C_N[3]]; # configured as out from SI5338 CLK3


set_property PACKAGE_PIN C8 [get_ports FMC_MGT_TX_P[9]]
set_property PACKAGE_PIN C7 [get_ports FMC_MGT_TX_N[9]]
set_property PACKAGE_PIN D6 [get_ports FMC_MGT_TX_P[8]]
set_property PACKAGE_PIN D5 [get_ports FMC_MGT_TX_N[8]]
set_property PACKAGE_PIN E4 [get_ports FMC_MGT_TX_P[7]]
set_property PACKAGE_PIN E3 [get_ports FMC_MGT_TX_N[7]]
set_property PACKAGE_PIN F6 [get_ports FMC_MGT_TX_P[6]]
set_property PACKAGE_PIN F5 [get_ports FMC_MGT_TX_N[6]]
set_property PACKAGE_PIN H6 [get_ports FMC_MGT_TX_P[5]]
set_property PACKAGE_PIN H5 [get_ports FMC_MGT_TX_N[5]]
set_property PACKAGE_PIN J4 [get_ports FMC_MGT_TX_P[4]]
set_property PACKAGE_PIN J3 [get_ports FMC_MGT_TX_N[4]]
set_property PACKAGE_PIN K6 [get_ports FMC_MGT_TX_P[3]]
set_property PACKAGE_PIN K5 [get_ports FMC_MGT_TX_N[3]]
set_property PACKAGE_PIN M6 [get_ports FMC_MGT_TX_P[2]]
set_property PACKAGE_PIN M5 [get_ports FMC_MGT_TX_N[2]]
set_property PACKAGE_PIN N4 [get_ports FMC_MGT_TX_P[1]]
set_property PACKAGE_PIN N3 [get_ports FMC_MGT_TX_N[1]]
set_property PACKAGE_PIN P6 [get_ports FMC_MGT_TX_P[0]]
set_property PACKAGE_PIN P5 [get_ports FMC_MGT_TX_N[0]]


set_property PACKAGE_PIN C4 [get_ports FMC_MGT_RX_P[9]]
set_property PACKAGE_PIN C3 [get_ports FMC_MGT_RX_N[9]]
set_property PACKAGE_PIN D2 [get_ports FMC_MGT_RX_P[8]]
set_property PACKAGE_PIN D1 [get_ports FMC_MGT_RX_N[8]]
set_property PACKAGE_PIN F2 [get_ports FMC_MGT_RX_P[7]]
set_property PACKAGE_PIN F1 [get_ports FMC_MGT_RX_N[7]]
set_property PACKAGE_PIN G4 [get_ports FMC_MGT_RX_P[6]]
set_property PACKAGE_PIN G3 [get_ports FMC_MGT_RX_N[6]]
set_property PACKAGE_PIN H2 [get_ports FMC_MGT_RX_P[5]]
set_property PACKAGE_PIN H1 [get_ports FMC_MGT_RX_N[5]]
set_property PACKAGE_PIN K2 [get_ports FMC_MGT_RX_P[4]]
set_property PACKAGE_PIN K1 [get_ports FMC_MGT_RX_N[4]]
set_property PACKAGE_PIN L4 [get_ports FMC_MGT_RX_P[3]]
set_property PACKAGE_PIN L3 [get_ports FMC_MGT_RX_N[3]]
set_property PACKAGE_PIN M2 [get_ports FMC_MGT_RX_P[2]]
set_property PACKAGE_PIN M1 [get_ports FMC_MGT_RX_N[2]]
set_property PACKAGE_PIN P2 [get_ports FMC_MGT_RX_P[1]]
set_property PACKAGE_PIN P1 [get_ports FMC_MGT_RX_N[1]]
set_property PACKAGE_PIN R4 [get_ports FMC_MGT_RX_P[0]]
set_property PACKAGE_PIN R3 [get_ports FMC_MGT_RX_N[0]]



set_property PACKAGE_PIN AG5 [get_ports FMC_HB_N[0]]; # CC
set_property PACKAGE_PIN AG6 [get_ports FMC_HB_P[0]]; # CC 
set_property PACKAGE_PIN AG3 [get_ports FMC_HB_N[1]]; 
set_property PACKAGE_PIN AF3 [get_ports FMC_HB_P[1]]; 
set_property PACKAGE_PIN AG1 [get_ports FMC_HB_N[2]]; # inverted
set_property PACKAGE_PIN AH1 [get_ports FMC_HB_P[2]]; # inverted
set_property PACKAGE_PIN AH2 [get_ports FMC_HB_N[3]]; 
set_property PACKAGE_PIN AH3 [get_ports FMC_HB_P[3]]; 
set_property PACKAGE_PIN AF1 [get_ports FMC_HB_N[4]]; 
set_property PACKAGE_PIN AF2 [get_ports FMC_HB_P[4]]; 
set_property PACKAGE_PIN AJ2 [get_ports FMC_HB_N[5]]; # inverted
set_property PACKAGE_PIN AK2 [get_ports FMC_HB_P[5]]; # inverted
set_property PACKAGE_PIN AD7  [get_ports FMC_HB_N[6]]; #cc
set_property PACKAGE_PIN AC7  [get_ports FMC_HB_P[6]];
set_property PACKAGE_PIN AC2  [get_ports FMC_HB_N[7]]; # inverted
set_property PACKAGE_PIN AD2  [get_ports FMC_HB_P[7]]; # inverted
set_property PACKAGE_PIN AE2  [get_ports FMC_HB_N[8]];
set_property PACKAGE_PIN AE3  [get_ports FMC_HB_P[8]];
set_property PACKAGE_PIN AA2  [get_ports FMC_HB_N[9]];
set_property PACKAGE_PIN AA3  [get_ports FMC_HB_P[9]];
set_property PACKAGE_PIN AA1 [get_ports FMC_HB_N[10]]; # inverted
set_property PACKAGE_PIN AB1 [get_ports FMC_HB_P[10]]; # inverted
set_property PACKAGE_PIN AC1 [get_ports FMC_HB_N[11]]; # inverted
set_property PACKAGE_PIN AD1 [get_ports FMC_HB_P[11]]; # inverted
set_property PACKAGE_PIN C12 [get_ports FMC_HB_N[12]] # inverted
set_property PACKAGE_PIN B11 [get_ports FMC_HB_P[12]] # inverted
set_property PACKAGE_PIN D12 [get_ports FMC_HB_N[13]]
set_property PACKAGE_PIN E12 [get_ports FMC_HB_P[13]]
set_property PACKAGE_PIN A11 [get_ports FMC_HB_N[14]]
set_property PACKAGE_PIN A12 [get_ports FMC_HB_P[14]]
set_property PACKAGE_PIN A10 [get_ports FMC_HB_N[15]]
set_property PACKAGE_PIN B10 [get_ports FMC_HB_P[15]]
set_property PACKAGE_PIN K12 [get_ports FMC_HB_N[16]]
set_property PACKAGE_PIN K13 [get_ports FMC_HB_P[16]]
set_property PACKAGE_PIN F10 [get_ports FMC_HB_N[17]] ##CC
set_property PACKAGE_PIN G10 [get_ports FMC_HB_P[17]]
set_property PACKAGE_PIN H11 [get_ports FMC_HB_N[18]]
set_property PACKAGE_PIN J12 [get_ports FMC_HB_P[18]]
set_property PACKAGE_PIN C11 [get_ports FMC_HB_N[19]]
set_property PACKAGE_PIN D11 [get_ports FMC_HB_P[19]]
set_property PACKAGE_PIN J11 [get_ports FMC_HB_N[20]]
set_property PACKAGE_PIN J10 [get_ports FMC_HB_P[20]]
set_property PACKAGE_PIN G11 [get_ports FMC_HB_N[21]]
set_property PACKAGE_PIN H12 [get_ports FMC_HB_P[21]]

set_property PACKAGE_PIN V6  [get_ports  FMC_HA_N[0]]; # CC
set_property PACKAGE_PIN V7  [get_ports  FMC_HA_P[0]]; # CC 
set_property PACKAGE_PIN V8  [get_ports  FMC_HA_N[1]]; # cc
set_property PACKAGE_PIN U8  [get_ports  FMC_HA_P[1]]; # cc 
set_property PACKAGE_PIN U3  [get_ports  FMC_HA_N[2]];
set_property PACKAGE_PIN U2  [get_ports  FMC_HA_P[2]];
set_property PACKAGE_PIN U1  [get_ports  FMC_HA_N[3]];
set_property PACKAGE_PIN T1  [get_ports  FMC_HA_P[3]];
set_property PACKAGE_PIN Y5  [get_ports  FMC_HA_N[4]];
set_property PACKAGE_PIN W5  [get_ports  FMC_HA_P[4]];
set_property PACKAGE_PIN U4  [get_ports  FMC_HA_N[5]]; 
set_property PACKAGE_PIN U5  [get_ports  FMC_HA_P[5]]; 
set_property PACKAGE_PIN W2  [get_ports  FMC_HA_N[6]];  # inverted
set_property PACKAGE_PIN W1  [get_ports  FMC_HA_P[6]]; # inverted
set_property PACKAGE_PIN V2  [get_ports  FMC_HA_N[7]]; # inverted
set_property PACKAGE_PIN V1  [get_ports  FMC_HA_P[7]]; # inverted
set_property PACKAGE_PIN L12 [get_ports  FMC_HA_N[8]]; # inverted
set_property PACKAGE_PIN L11 [get_ports  FMC_HA_P[8]]; # inverted
set_property PACKAGE_PIN N12 [get_ports  FMC_HA_N[9]]; # inverted
set_property PACKAGE_PIN M12 [get_ports  FMC_HA_P[9]]; # inverted
set_property PACKAGE_PIN V9  [get_ports FMC_HA_N[10]];
set_property PACKAGE_PIN U9  [get_ports FMC_HA_P[10]];
set_property PACKAGE_PIN Y2  [get_ports FMC_HA_N[11]]; # inverted
set_property PACKAGE_PIN Y1  [get_ports FMC_HA_P[11]]; # inverted
set_property PACKAGE_PIN W11 [get_ports FMC_HA_N[12]]; # inverted
set_property PACKAGE_PIN W10 [get_ports FMC_HA_P[12]]; # inverted
set_property PACKAGE_PIN W6  [get_ports FMC_HA_N[13]];
set_property PACKAGE_PIN W7  [get_ports FMC_HA_P[13]];
set_property PACKAGE_PIN N10 [get_ports FMC_HA_N[14]]; # inverted
set_property PACKAGE_PIN M10 [get_ports FMC_HA_P[14]]; # inverted
set_property PACKAGE_PIN W4  [get_ports FMC_HA_N[15]];
set_property PACKAGE_PIN V4  [get_ports FMC_HA_P[15]];
set_property PACKAGE_PIN P11 [get_ports FMC_HA_N[16]]; # inverted
set_property PACKAGE_PIN P10 [get_ports FMC_HA_P[16]]; # inverted
set_property PACKAGE_PIN Y8  [get_ports FMC_HA_N[17]]; ##CC
set_property PACKAGE_PIN Y9  [get_ports FMC_HA_P[17]];
set_property PACKAGE_PIN R10 [get_ports FMC_HA_N[18]]; # inverted
set_property PACKAGE_PIN T10 [get_ports FMC_HA_P[18]]; # inverted
set_property PACKAGE_PIN L10 [get_ports FMC_HA_N[19]]; # inverted
set_property PACKAGE_PIN K10 [get_ports FMC_HA_P[19]]; # inverted
set_property PACKAGE_PIN V11 [get_ports FMC_HA_N[20]];
set_property PACKAGE_PIN U11 [get_ports FMC_HA_P[20]];
set_property PACKAGE_PIN U10 [get_ports FMC_HA_N[21]];
set_property PACKAGE_PIN T11 [get_ports FMC_HA_P[21]];
set_property PACKAGE_PIN U7  [get_ports FMC_HA_N[22]]; # inverted
set_property PACKAGE_PIN U6  [get_ports FMC_HA_P[22]]; # inverted
set_property PACKAGE_PIN Y3  [get_ports FMC_HA_N[23]]; 
set_property PACKAGE_PIN Y4  [get_ports FMC_HA_P[23]]; 

set_property PACKAGE_PIN AH8  [get_ports FMC_LA_N[0]]; # CC
set_property PACKAGE_PIN AG8  [get_ports FMC_LA_P[0]]; # CC 
set_property PACKAGE_PIN AJ7  [get_ports FMC_LA_N[1]]; # cc
set_property PACKAGE_PIN AH7  [get_ports FMC_LA_P[1]]; # cc 
set_property PACKAGE_PIN AF8  [get_ports FMC_LA_N[2]]; # inverted
set_property PACKAGE_PIN AF7  [get_ports FMC_LA_P[2]]; # inverted
set_property PACKAGE_PIN AG10 [get_ports FMC_LA_N[3]];
set_property PACKAGE_PIN AF10 [get_ports FMC_LA_P[3]];
set_property PACKAGE_PIN AH13 [get_ports FMC_LA_N[4]];
set_property PACKAGE_PIN AG13 [get_ports FMC_LA_P[4]];
set_property PACKAGE_PIN AJ12 [get_ports FMC_LA_N[5]];
set_property PACKAGE_PIN AH12 [get_ports FMC_LA_P[5]];
set_property PACKAGE_PIN AG11 [get_ports FMC_LA_N[6]]; 
set_property PACKAGE_PIN AF11 [get_ports FMC_LA_P[6]];
set_property PACKAGE_PIN AJ9  [get_ports FMC_LA_N[7]];
set_property PACKAGE_PIN AH9  [get_ports FMC_LA_P[7]];
set_property PACKAGE_PIN AK13 [get_ports FMC_LA_N[8]]; # inverted
set_property PACKAGE_PIN AK12 [get_ports FMC_LA_P[8]]; # inverted
set_property PACKAGE_PIN AK10 [get_ports FMC_LA_N[9]];
set_property PACKAGE_PIN AJ10 [get_ports FMC_LA_P[9]];
set_property PACKAGE_PIN AJ11 [get_ports FMC_LA_N[10]]; # inverted
set_property PACKAGE_PIN AK11 [get_ports FMC_LA_P[10]]; # inverted
set_property PACKAGE_PIN AK8  [get_ports FMC_LA_N[11]];
set_property PACKAGE_PIN AK9  [get_ports FMC_LA_P[11]];
set_property PACKAGE_PIN AF5  [get_ports FMC_LA_N[12]];
set_property PACKAGE_PIN AF6  [get_ports FMC_LA_P[12]];
set_property PACKAGE_PIN AH4  [get_ports FMC_LA_N[13]]; # inverted
set_property PACKAGE_PIN AJ4  [get_ports FMC_LA_P[13]]; # inverted
set_property PACKAGE_PIN AJ5  [get_ports FMC_LA_N[14]]; # inverted
set_property PACKAGE_PIN AK5  [get_ports FMC_LA_P[14]]; # inverted
set_property PACKAGE_PIN AK7  [get_ports FMC_LA_N[15]]; # inverted
set_property PACKAGE_PIN AK6  [get_ports FMC_LA_P[15]]; # inverted
set_property PACKAGE_PIN AK3  [get_ports FMC_LA_N[16]]; 
set_property PACKAGE_PIN AK4  [get_ports FMC_LA_P[16]]; 
set_property PACKAGE_PIN AB5  [get_ports FMC_LA_N[17]]; ##CC
set_property PACKAGE_PIN AB6  [get_ports FMC_LA_P[17]];
set_property PACKAGE_PIN AC8  [get_ports FMC_LA_N[18]]; ##cc
set_property PACKAGE_PIN AB8  [get_ports FMC_LA_P[18]];
set_property PACKAGE_PIN AF13 [get_ports FMC_LA_N[19]];
set_property PACKAGE_PIN AE13 [get_ports FMC_LA_P[19]];
set_property PACKAGE_PIN AE9  [get_ports FMC_LA_N[20]];
set_property PACKAGE_PIN AD9  [get_ports FMC_LA_P[20]];
set_property PACKAGE_PIN AB4  [get_ports FMC_LA_N[21]]; # inverted
set_property PACKAGE_PIN AC4  [get_ports FMC_LA_P[21]]; # inverted
set_property PACKAGE_PIN AA5  [get_ports FMC_LA_N[22]];
set_property PACKAGE_PIN AA6  [get_ports FMC_LA_P[22]];
set_property PACKAGE_PIN AC9  [get_ports FMC_LA_N[23]];
set_property PACKAGE_PIN AB9  [get_ports FMC_LA_P[23]];
set_property PACKAGE_PIN AE4  [get_ports FMC_LA_N[24]];
set_property PACKAGE_PIN AD4  [get_ports FMC_LA_P[24]];
set_property PACKAGE_PIN AC11 [get_ports FMC_LA_N[25]]; # inverted
set_property PACKAGE_PIN AD11 [get_ports FMC_LA_P[25]]; # inverted
set_property PACKAGE_PIN AB10 [get_ports FMC_LA_N[26]];
set_property PACKAGE_PIN AB11 [get_ports FMC_LA_P[26]];
set_property PACKAGE_PIN AA7  [get_ports FMC_LA_N[27]];
set_property PACKAGE_PIN AA8  [get_ports FMC_LA_P[27]];
set_property PACKAGE_PIN AB3  [get_ports FMC_LA_N[28]]; # inverted
set_property PACKAGE_PIN AC3  [get_ports FMC_LA_P[28]]; # inverted
set_property PACKAGE_PIN AE10 [get_ports FMC_LA_N[29]];
set_property PACKAGE_PIN AD10 [get_ports FMC_LA_P[29]];
set_property PACKAGE_PIN AD12 [get_ports FMC_LA_N[30]]; # inverted
set_property PACKAGE_PIN AE12 [get_ports FMC_LA_P[30]]; # inverted
set_property PACKAGE_PIN AE5  [get_ports FMC_LA_N[31]];
set_property PACKAGE_PIN AD5  [get_ports FMC_LA_P[31]];
set_property PACKAGE_PIN AA11 [get_ports FMC_LA_N[32]];
set_property PACKAGE_PIN AA12 [get_ports FMC_LA_P[32]];
set_property PACKAGE_PIN AC13 [get_ports FMC_LA_N[33]]; 
set_property PACKAGE_PIN AB13 [get_ports FMC_LA_P[33]]; 


