# Real Vivado IP validation of the three Gen2 PS presets and GPIO interfaces.
# Run from any directory; generated files go under SDK tmp/.
set sdk_path [file normalize [file join [file dirname [info script]] .. .. ..]]
source $sdk_path/fpga/lib/utilities.tcl
proc expect {actual expected label} {
  if {$actual ne $expected} {error "$label: expected '$expected', got '$actual'"}
}
foreach {board part bus high gpio_width} {
  red-pitaya-gen2 xc7z010clg400-1 {16 Bit} 0x1FFFFFFF 8
  red-pitaya-pro-gen2 xc7z010clg400-1 {16 Bit} 0x1FFFFFFF 8
  red-pitaya-pro-z7020-gen2 xc7z020clg400-1 {32 Bit} 0x3FFFFFFF 11
} {
  create_project -force -part $part bsp_check $sdk_path/tmp/bsp-check/$board
  create_bd_design system
  set board_path $sdk_path/boards/$board
  namespace eval config {variable fclk0 50000000}
  set board_preset $board_path/config/board_preset.tcl
  source $sdk_path/fpga/lib/starting_point.tcl
  foreach {name value} [list \
    PCW_UIPARAM_DDR_BUS_WIDTH $bus \
    PCW_UIPARAM_DDR_DRAM_WIDTH {16 Bits} \
    PCW_DDR_RAM_HIGHADDR $high \
    PCW_SD0_GRP_CD_ENABLE 0 PCW_SD0_GRP_WP_ENABLE 0 \
    PCW_QSPI_PERIPHERAL_ENABLE 0 \
    PCW_UART0_UART0_IO {MIO 14 .. 15} \
    PCW_I2C0_I2C0_IO {MIO 50 .. 51} \
    PCW_ENET0_ENET0_IO {MIO 16 .. 27} \
    PCW_SD0_SD0_IO {MIO 40 .. 45} \
  ] {
    expect [get_property CONFIG.$name [get_bd_cells $ps_name]] $value "$board $name"
  }
  source $board_path/gpio.tcl
  add_gpio $gpio_width
  # An excessive width must be rejected before creating another GPIO cell.
  expect [catch {add_gpio [expr {$gpio_width + 1}]}] 1 "$board GPIO width limit"
  assign_bd_address
  validate_bd_design
  expect [get_property CONFIG.C_GPIO_WIDTH [get_bd_cells axi_gpio_0]] $gpio_width "$board GPIO width"
  if {$gpio_width == 11} {
    source $board_path/e3.tcl
    add_e3_ports
    expect [llength [get_bd_ports exp_e3*]] 4 "$board E3 port groups"
    expect [llength [get_bd_ports s1_*]] 2 "$board S1 status inputs"
  }
  save_bd_design
  puts "BSP CHECK PASSED: $board"
  close_project
}
