proc add_address_module {module_name bram_width clk} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I                  clk
  create_bd_pin -dir I -from 32   -to 0 cfg
  create_bd_pin -dir O -from 15   -to 0 addr
  create_bd_pin -dir O -from 15   -to 0 addr_delayed
  create_bd_pin -dir O                  restart
  create_bd_pin -dir O                  tvalid

  # Add address counter
  cell xilinx.com:ip:c_counter_binary:12.0 base_counter {
    Output_Width [expr $bram_width+2]
    Increment_Value 4
    SCLR true
  } {
    CLK clk
    Q addr
  }

  cell koheron:user:edge_detector:1.0 reset_base_counter {
  } {
    clk clk
    dout base_counter/SCLR
  }

  cell koheron:user:edge_detector:1.0 edge_detector {
  } { 
    clk clk
    dout restart
  }

  cell xilinx.com:ip:c_shift_ram:12.0 delay_addr {
    ShiftRegType Variable_Length_Lossless
    Width [expr $bram_width+2]
  } {
    D base_counter/Q
    CLK clk
    Q addr_delayed
  }

  # Configuration registers

  cell xilinx.com:ip:xlslice:1.0 reset_base_counter_slice {
    DIN_WIDTH 32
    DIN_FROM 0
    DIN_TO 0
  } {
    Din cfg
    Dout reset_base_counter/din
  }

  cell xilinx.com:ip:xlslice:1.0 start_slice {
    DIN_WIDTH 32
    DIN_FROM 1
    DIN_TO 1
  } {
    Din cfg
    Dout edge_detector/Din
  }

  cell xilinx.com:ip:xlslice:1.0 addr_delay_slice {
    DIN_WIDTH 32
    DIN_FROM 5
    DIN_TO 2
  } {
    Din cfg
    Dout delay_addr/A
  }

  #

  connect_pins reset_base_counter_slice/Dout tvalid

  current_bd_instance $bd

}
