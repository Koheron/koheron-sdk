source $board_path/starting_point.tcl
source $sdk_path/fpga/modules/vector-network-analyzer.tcl
vector_network_analyzer adc_dac/adc0 adc_dac/adc_clk rst_adc_clk/peripheral_aresetn
connect_pins capture/dac adc_dac/dac0
connect_pins adc_dac/dac1 [get_constant_pin 0 16]
validate_bd_design
