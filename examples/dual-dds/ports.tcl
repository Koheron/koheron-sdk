
### ADC
create_bd_port -dir I -from 13 -to 0 adc_dat_a_i
create_bd_port -dir I -from 13 -to 0 adc_dat_b_i
create_bd_port -dir I adc_clk_p_i
create_bd_port -dir I adc_clk_n_i
create_bd_port -dir O -from 1 -to 0 adc_clk_source
create_bd_port -dir O adc_cdcs_o

### DAC
create_bd_port -dir O -from 13 -to 0 dac_dat_o
create_bd_port -dir O dac_clk_o
create_bd_port -dir O dac_rst_o
create_bd_port -dir O dac_sel_o
create_bd_port -dir O dac_wrt_o

### LED
create_bd_port -dir O -from 7 -to 0 led_o
