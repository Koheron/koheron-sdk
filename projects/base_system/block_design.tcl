source boards/$board_name/base_system.tcl

# Add DAC BRAM
source scripts/bram.tcl
set bram_name dac_bram
add_bram dac_bram 8K
# Connect port B of BRAM to ADC clock
connect_bd_net [get_bd_pins blk_mem_gen_$bram_name/clkb] [get_bd_pins adc_dac_0/adc_clk_o]

set n_bits_bram 13
cell xilinx.com:ip:c_counter_binary:12.0 base_counter \
  [list Output_Width [expr $n_bits_bram+3] Increment_Value 4] \
  [list CLK adc_dac_0/adc_clk_o Q blk_mem_gen_$bram_name/addrb]

# Connect BRAM output to DACs
for {set i 0} {$i < 2} {incr i} {
  set channel [lindex {a b} $i]
  cell xilinx.com:ip:xlslice:1.0 dac_${channel}_slice \
    [list DIN_WIDTH 32 DIN_FROM [expr 13+16*$i] DIN_TO [expr 16*$i]] \
    [list Din blk_mem_gen_$bram_name/doutb Dout adc_dac_0/dac_dat_${channel}_i]
}

# Connect remaining ports of BRAM
cell xilinx.com:ip:xlconstant:1.1 ${bram_name}_dinb {CONST_VAL 0 CONST_WIDTH 32} [list dout blk_mem_gen_$bram_name/dinb]
cell xilinx.com:ip:xlconstant:1.1 ${bram_name}_enb {CONST_VAL 1} [list dout blk_mem_gen_$bram_name/enb]
cell xilinx.com:ip:xlconstant:1.1 ${bram_name}_web {CONST_VAL 0 CONST_WIDTH 4} [list dout blk_mem_gen_$bram_name/web]
connect_bd_net [get_bd_pins blk_mem_gen_$bram_name/rstb] [get_bd_pins adc_rst/dout]
