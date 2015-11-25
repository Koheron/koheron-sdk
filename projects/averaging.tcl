
proc add_averaging_module {module_name bram_addr_width adc_witdh clk} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I                                       clk
  create_bd_pin -dir I                                       start
  create_bd_pin -dir I -from 32                        -to 0 cfg
  create_bd_pin -dir I -from [expr $adc_witdh-1]       -to 0 data_in
  create_bd_pin -dir I -from [expr $bram_addr_width+1] -to 0 addr
  create_bd_pin -dir O -from 31                        -to 0 data_out
  create_bd_pin -dir O -from 3                         -to 0 wen
  create_bd_pin -dir O -from 31                        -to 0 count_cycle

  connect_bd_net [get_bd_pins clk] [get_bd_pins /$clk]

  ## Add FIFO

  cell xilinx.com:ip:fifo_generator:13.0 fifo     \
    [list                                         \
      Input_Data_Width 32                         \
      Input_Depth      [expr 2**$bram_addr_width] \
      Data_Count       true                       \
      Data_Count_Width $bram_addr_width           \
      Reset_Pin        false]                     \
    [list clk clk dout data_out]

  cell xilinx.com:ip:c_addsub:12.0 adder \
    [list                                \
      A_Width.VALUE_SRC USER             \
      B_Width.VALUE_SRC USER             \
      A_Width           32               \
      B_Width           $adc_witdh       \
      Out_Width         32               \
      CE                false            \
      Latency           3                \
      Reset_Pin         false]           \
    [list CLK clk B data_in S fifo/din]

  cell xilinx.com:ip:xlconstant:1.1 wr_en_one {} {dout fifo/wr_en}

  cell pavel-demin:user:comparator:1.0 comp \
    [list DATA_WIDTH $bram_addr_width] {
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

  cell xilinx.com:ip:xlslice:1.0 address_slice \
    [list                                      \
      DIN_WIDTH [expr $bram_addr_width+2]      \
      DIN_FROM  [expr $bram_addr_width+1]      \
      DIN_TO    2]                             \
    [list Din addr]

  cell pavel-demin:user:write_enable:1.0 write_enable \
    [list BRAM_WIDTH $bram_addr_width]                \
    [list                                             \
      start_acq   start                               \
      clk         clk                                 \
      address     address_slice/Dout                  \
      count_cycle count_cycle
    ]

  cell xilinx.com:ip:c_shift_ram:12.0 delay_wen_int {
    ShiftRegType Variable_Length_Lossless
    Width 1
  } [list D write_enable/wen CLK clk]

  cell xilinx.com:ip:xlslice:1.0 wen_slice {DIN_WIDTH 4} {Din delay_wen_int/Q Dout wen_or_avg_on/Op1}

  cell xilinx.com:ip:c_shift_ram:12.0 delay_wen_ext {
    ShiftRegType Variable_Length_Lossless
    Width 1
  } [list D write_enable/wen CLK clk]

  cell xilinx.com:ip:xlconcat:2.1 concat_wen {NUM_PORTS 4} {dout wen}

  for {set i 0} {$i < 4} {incr i} {
    connect_pins concat_wen/In$i delay_wen_ext/Q
  }

  # Configuration registers

  cell xilinx.com:ip:xlslice:1.0 comp_slice \
    [list                                   \
      DIN_WIDTH 32                          \
      DIN_FROM  [expr $bram_addr_width-1]   \
      DIN_TO    0]                          \
    [list Din cfg Dout comp/b]

  cell xilinx.com:ip:xlslice:1.0 avg_on_slice \
    [list                                     \
      DIN_WIDTH 32                            \
      DIN_FROM  [expr $bram_addr_width]       \
      DIN_TO    [expr $bram_addr_width]]      \
    [list Din cfg Dout wen_or_avg_on/Op2]

  cell xilinx.com:ip:xlslice:1.0 wen_int_delay_slice \
    [list                                            \
      DIN_WIDTH 32                                   \
      DIN_FROM  [expr $bram_addr_width+4]            \
      DIN_TO    [expr $bram_addr_width+1]]           \
    [list Din cfg Dout delay_wen_int/A]

  cell xilinx.com:ip:xlslice:1.0 wen_ext_delay_slice \
    [list                                            \
      DIN_WIDTH 32                                   \
      DIN_FROM  [expr $bram_addr_width+8]            \
      DIN_TO    [expr $bram_addr_width+5]]           \
    [list Din cfg Dout delay_wen_ext/A]

  #

  current_bd_instance $bd

}
