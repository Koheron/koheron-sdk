proc add_address_module {module_name bram_width clk} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I                  clk
  create_bd_pin -dir I -from 32   -to 0 cfg
  create_bd_pin -dir I -from 32   -to 0 period
  create_bd_pin -dir O -from [expr $bram_width+2]   -to 0 addr
  create_bd_pin -dir O                  restart
  create_bd_pin -dir O                  tvalid

  # Add address counter
  cell koheron:user:address_generator:1.0 base_counter {
    COUNT_WIDTH $bram_width
  } {
    clk clk
    count_max period
    address addr
  }

  cell koheron:user:edge_detector:1.0 reset_base_counter {
  } {
    clk clk
    dout base_counter/sclr
  }

  cell koheron:user:edge_detector:1.0 edge_detector {
  } { 
    clk clk
    dout restart
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

  cell xilinx.com:ip:c_shift_ram:12.0 delay_tvalid {
    Depth 1
    Width 1
  } {
    D reset_base_counter_slice/Dout
    CLK clk
    Q tvalid
  }

  current_bd_instance $bd

}
