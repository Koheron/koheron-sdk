proc add_sata {module_name} {
  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I -type clk clk
  create_bd_pin -dir I sata_data_in_p
  create_bd_pin -dir I sata_data_in_n
  create_bd_pin -dir I din
  create_bd_pin -dir I -from 31 -to 0 ctl
  create_bd_pin -dir O sata_data_out_p
  create_bd_pin -dir O sata_data_out_n
  create_bd_pin -dir O dout
  create_bd_intf_pin -mode Master -vlnv xilinx.com:interface:diff_clock_rtl:1.0 sata_clk_out

  cell xilinx.com:ip:selectio_wiz:5.1 sata_in {
    BUS_SIG_TYPE DIFF
    BUS_IO_STD DIFF_HSTL_I_18
    SELIO_ACTIVE_EDGE SDR
    SELIO_CLK_BUF MMCM
  } {
    data_in_from_pins_p sata_data_in_p
    data_in_from_pins_n sata_data_in_n
    clk_in clk
  }

  cell xilinx.com:ip:selectio_wiz:5.1 sata_out {
    BUS_DIR OUTPUTS
    BUS_SIG_TYPE DIFF
    BUS_IO_STD DIFF_HSTL_I_18
    SELIO_ACTIVE_EDGE SDR
    SELIO_CLK_BUF MMCM
    CLK_FWD true
  } {
    data_out_to_pins_p sata_data_out_p
    data_out_to_pins_n sata_data_out_n
    clk_in clk
    diff_clk_to_pins sata_clk_out
  }

  # Mux
  cell koheron:user:bus_multiplexer:1.0 sata_mux {
    WIDTH 1
  } {
    sel [get_slice_pin ctl 0 0]
    din0 sata_in/data_in_to_device
    din1 din
    dout sata_out/data_out_from_device
  }

  cell xilinx.com:ip:c_shift_ram:12.0 trig_delay {
    ShiftRegType Variable_Length_Lossless
    Width 1
  } {
    CLK clk
    D sata_in/data_in_to_device
    A [get_slice_pin ctl 4 1]
    Q dout
  }

  current_bd_instance $bd
}
