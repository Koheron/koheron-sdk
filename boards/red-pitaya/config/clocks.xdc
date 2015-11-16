
#create_clock -period 8.000 -name clk_out1_system_pll_0 [get_ports adc_clk_p_i]

#set_input_delay -clock adc_clk 3.400 [get_ports adc_dat_a_i[*]]
#set_input_delay -clock adc_clk 3.400 [get_ports adc_dat_b_i[*]]

#create_clock -period 4.000 -name rx_clk  [get_ports daisy_p_i[1]]

set_false_path -from [get_clocks clk_out1_system_pll_0]     -to [get_clocks clk_out2_system_pll_0]
#set_false_path -from [get_clocks clk_fpga_0]  -to [get_clocks ser_clk_out]
set_false_path -from [get_clocks clk_fpga_0]  -to [get_clocks clk_out3_system_pll_0]
set_false_path -from [get_clocks clk_fpga_0]  -to [get_clocks clk_out1_system_pll_0]
#set_false_path -from [get_clocks clk_fpga_0]  -to [get_clocks par_clk]
set_false_path -from [get_clocks clk_out2_system_pll_0] -to [get_clocks clk_out3_system_pll_0]
set_false_path -from [get_clocks clk_out1_system_pll_0] -to [get_clocks clk_out4_system_pll_0]

set_false_path -from [get_clocks clk_fpga_0]  -to [get_clocks clk_out6_system_pll_0]
