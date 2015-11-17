set module_name averaging
set bd [current_bd_instance .]
current_bd_instance [create_bd_cell -type hier $module_name]

## Add FIFO

cell xilinx.com:ip:fifo_generator:13.0 fifo {
  Input_Data_Width 32
  Input_Depth      8192
  Data_Count       true
  Data_Count_Width 13
} {clk /pll/clk_out1}

connect_bd_net [get_bd_pins /blk_mem_gen_$adc1_bram_name/dinb] [get_bd_pins fifo/dout]

cell xilinx.com:ip:xlconcat:2.1 concat_adc_a {} {dout fifo/din In0 /adc_0/adc_dat_a_o}

cell xilinx.com:ip:xlconstant:1.1 zero_18bits {CONST_WIDTH 18 CONST_VAL 0} {dout concat_adc_a/In1}

cell pavel-demin:user:comparator:1.0 comp {DATA_WIDTH 13} {
  a fifo/data_count
  a_geq_b fifo/rd_en
}

set comp_offset [expr 5*32]
cell xilinx.com:ip:xlslice:1.0 comp_slice \
  [list DIN_WIDTH 1024 DIN_FROM [expr 12+$comp_offset] DIN_TO [expr $comp_offset]] \
  [list Din /axi_cfg_register_0/cfg_data Dout comp/b]

set start_offset [expr 6*32]
cell xilinx.com:ip:xlslice:1.0 start_slice \
  [list DIN_WIDTH 1024 DIN_FROM [expr $start_offset] DIN_TO [expr $start_offset]] \
  [list Din /axi_cfg_register_0/cfg_data]

cell pavel-demin:user:edge_detector:1.0 edge_detector {} {din start_slice/Dout clk /pll/clk_out1}

cell pavel-demin:user:write_enable:1.0 write_enable [list BRAM_WIDTH $bram_width] {
  start_acq edge_detector/dout
  clk /pll/clk_out1
  address /base_counter/Q
}
connect_bd_net [get_bd_pins write_enable/wen] [get_bd_pins /blk_mem_gen_$adc1_bram_name/web]

current_bd_instance $bd
