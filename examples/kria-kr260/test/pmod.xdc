# KR260 Robotics Starter Kit - PMOD constraints for 16x UART Lite design
# Voltage: LVCMOS33 for all PMOD signals on these headers
#
# Mapping convention used here:
# - PMOD1 carries UART0..UART3 (TX/RX pairs on pins 1..8)
# - PMOD2 carries UART4..UART7 (TX/RX pairs on pins 1..8)
# - PMOD3 carries UART8..UART11 (TX/RX pairs on pins 1..8)
# - PMOD4 carries UART12..UART15 (TX/RX pairs on pins 1..8)

## =========================================================
## PMOD #1
## =========================================================
set_property PACKAGE_PIN H12 [get_ports {pmod_uart_tx_00}]
set_property PACKAGE_PIN E10 [get_ports {pmod_uart_rx_00}]
set_property PACKAGE_PIN D10 [get_ports {pmod_uart_tx_01}]
set_property PACKAGE_PIN C11 [get_ports {pmod_uart_rx_01}]
set_property PACKAGE_PIN B10 [get_ports {pmod_uart_tx_02}]
set_property PACKAGE_PIN E12 [get_ports {pmod_uart_rx_02}]
set_property PACKAGE_PIN D11 [get_ports {pmod_uart_tx_03}]
set_property PACKAGE_PIN B11 [get_ports {pmod_uart_rx_03}]

## =========================================================
## PMOD #2
## =========================================================
set_property PACKAGE_PIN J11 [get_ports {pmod_uart_tx_04}]
set_property PACKAGE_PIN J10 [get_ports {pmod_uart_rx_04}]
set_property PACKAGE_PIN K13 [get_ports {pmod_uart_tx_05}]
set_property PACKAGE_PIN K12 [get_ports {pmod_uart_rx_05}]
set_property PACKAGE_PIN H11 [get_ports {pmod_uart_tx_06}]
set_property PACKAGE_PIN G10 [get_ports {pmod_uart_rx_06}]
set_property PACKAGE_PIN F12 [get_ports {pmod_uart_tx_07}]
set_property PACKAGE_PIN F11 [get_ports {pmod_uart_rx_07}]

## =========================================================
## PMOD #3
## =========================================================
set_property PACKAGE_PIN AE12 [get_ports {pmod_uart_tx_08}]
set_property PACKAGE_PIN AF12 [get_ports {pmod_uart_rx_08}]
set_property PACKAGE_PIN AG10 [get_ports {pmod_uart_tx_09}]
set_property PACKAGE_PIN AH10 [get_ports {pmod_uart_rx_09}]
set_property PACKAGE_PIN AF11 [get_ports {pmod_uart_tx_10}]
set_property PACKAGE_PIN AG11 [get_ports {pmod_uart_rx_10}]
set_property PACKAGE_PIN AH12 [get_ports {pmod_uart_tx_11}]
set_property PACKAGE_PIN AH11 [get_ports {pmod_uart_rx_11}]

## =========================================================
## PMOD #4
## =========================================================
set_property PACKAGE_PIN AC12 [get_ports {pmod_uart_tx_12}]
set_property PACKAGE_PIN AD12 [get_ports {pmod_uart_rx_12}]
set_property PACKAGE_PIN AE10 [get_ports {pmod_uart_tx_13}]
set_property PACKAGE_PIN AF10 [get_ports {pmod_uart_rx_13}]
set_property PACKAGE_PIN AD11 [get_ports {pmod_uart_tx_14}]
set_property PACKAGE_PIN AD10 [get_ports {pmod_uart_rx_14}]
set_property PACKAGE_PIN AA11 [get_ports {pmod_uart_tx_15}]
set_property PACKAGE_PIN AA10 [get_ports {pmod_uart_rx_15}]

set_property IOSTANDARD LVCMOS33 [get_ports {pmod_uart_tx_* pmod_uart_rx_*}]
set_property DRIVE 8 [get_ports pmod_uart_tx_*]
set_property SLEW FAST [get_ports {pmod_uart_tx_* pmod_uart_rx_*}]