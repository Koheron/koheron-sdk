set module_name averaging
set bd [current_bd_instance .]
current_bd_instance [create_bd_cell -type hier $module_name]

create_bd_pin -dir I clk
create_bd_pin -dir I -from 1023 -to 0 cfg
create_bd_pin -dir I -from 13   -to 0 data_in
create_bd_pin -dir I -from 14   -to 0 addr
create_bd_pin -dir O -from 31   -to 0 data_out
create_bd_pin -dir O -from 3    -to 0 wen


## Add FIFO

cell xilinx.com:ip:fifo_generator:13.0 fifo {
  Input_Data_Width 32
  Input_Depth      8192
  Data_Count       true
  Data_Count_Width 13
  Reset_Pin false
} [list clk clk dout data_out]

cell xilinx.com:ip:c_addsub:12.0 adder {
  A_Width.VALUE_SRC USER
  B_Width.VALUE_SRC USER
  A_Width 32
  B_Width 14
  Out_Width 32
  CE false
  Latency 3
  Reset_Pin false
} [list CLK clk B data_in S fifo/din]

cell xilinx.com:ip:xlconstant:1.1 wr_en_one {} {dout fifo/wr_en}

cell pavel-demin:user:comparator:1.0 comp {DATA_WIDTH 13} {
  a fifo/data_count
  a_geq_b fifo/rd_en
}

cell xilinx.com:ip:c_shift_ram:12.0 shift_reg {
  Width.VALUE_SRC USER
  Width 32
  Depth 1
  SCLR true
} [list CLK clk Q adder/A D fifo/dout]

cell xilinx.com:ip:util_vector_logic:2.0 wen_or_avg_on {
  C_SIZE 1
  C_OPERATION or
} {Res shift_reg/SCLR}

set averaging_offset [expr 5*32]
cell xilinx.com:ip:xlslice:1.0 comp_slice \
  [list DIN_WIDTH 1024 DIN_FROM [expr 12+$averaging_offset] DIN_TO [expr $averaging_offset]] \
  [list Din cfg Dout comp/b]

cell xilinx.com:ip:xlslice:1.0 avg_on_slice \
  [list DIN_WIDTH 1024 DIN_FROM [expr 13+$averaging_offset] DIN_TO [expr 13+$averaging_offset]] \
  [list Din cfg Dout wen_or_avg_on/Op2]

set start_offset [expr $reset_offset+1]
cell xilinx.com:ip:xlslice:1.0 start_slice \
  [list DIN_WIDTH 1024 DIN_FROM [expr $start_offset] DIN_TO [expr $start_offset]] \
  [list Din cfg]

cell pavel-demin:user:edge_detector:1.0 edge_detector {} [list din start_slice/Dout clk clk]

cell xilinx.com:ip:xlslice:1.0 address_slice \
  [list DIN_WIDTH 15 DIN_FROM 14 DIN_TO 2] \
  [list Din addr]

cell pavel-demin:user:write_enable:1.0 write_enable [list BRAM_WIDTH $bram_width] \
  [list start_acq edge_detector/dout clk clk address address_slice/Dout wen wen]



cell xilinx.com:ip:c_shift_ram:12.0 delay_wen {
  ShiftRegType Variable_Length_Lossless
  Width 1
} [list D write_enable/wen CLK clk]

cell xilinx.com:ip:xlslice:1.0 wen_delay_slice \
  [list DIN_WIDTH 1024 DIN_FROM [expr 17+$averaging_offset] DIN_TO [expr 14+$averaging_offset]] \
  [list Din cfg Dout delay_wen/A]

cell xilinx.com:ip:xlslice:1.0 wen_slice {DIN_WIDTH 4} {Din delay_wen/Q Dout wen_or_avg_on/Op1}

current_bd_instance $bd
