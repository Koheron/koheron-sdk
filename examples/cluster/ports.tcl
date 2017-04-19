
### ADC
create_bd_port -dir I -from 13 -to 0 adc_dat_a_i
create_bd_port -dir I -from 13 -to 0 adc_dat_b_i

create_bd_port -dir O -from 1 -to 0 adc_clk_source
create_bd_port -dir O adc_cdcs_o

create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 crystal_clk
set_property -dict [list CONFIG.FREQ_HZ [get_parameter adc_clk]] [get_bd_intf_ports crystal_clk]

### DAC
create_bd_port -dir O -from 13 -to 0 dac_dat_o
create_bd_port -dir O dac_clk_o
create_bd_port -dir O dac_rst_o
create_bd_port -dir O dac_sel_o
create_bd_port -dir O dac_wrt_o

### LED
create_bd_port -dir O -from 7 -to 0 led_o

create_bd_port -dir I dio1p
create_bd_port -dir O dio2p

### SATA connector
create_bd_intf_port -mode Slave  -vlnv xilinx.com:interface:diff_clock_rtl:1.0 sata_clk_in
create_bd_intf_port -mode Master -vlnv xilinx.com:interface:diff_clock_rtl:1.0 sata_clk_out

set_property -dict [list CONFIG.FREQ_HZ [get_parameter adc_clk]] [get_bd_intf_ports sata_clk_in]
set_property -dict [list CONFIG.FREQ_HZ [get_parameter adc_clk]] [get_bd_intf_ports sata_clk_out]

create_bd_port -dir I sata_data_in_p
create_bd_port -dir I sata_data_in_n

create_bd_port -dir O sata_data_out_p
create_bd_port -dir O sata_data_out_n
