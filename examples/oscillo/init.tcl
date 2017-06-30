source $board_path/config/ports.tcl
source $board_path/base_system.tcl

# Add pulse density modulator for laser current control
cell koheron:user:pdm:1.0 laser_current_pdm {
  NBITS [get_parameter pwm_width]
} {
  clk adc_dac/pwm_clk
  rst $rst_adc_clk_name/peripheral_reset
}
connect_port_pin dac_pwm_o [get_concat_pin [list [get_constant_pin 0 3] laser_current_pdm/dout]]

# Connect laser shutdown pin and reset overvoltage protection
create_bd_port -dir O laser_shutdown
create_bd_port -dir O laser_reset_overvoltage

connect_port_pin laser_shutdown [get_slice_pin [ctl_pin laser_control] 0 0]
connect_port_pin laser_reset_overvoltage [get_slice_pin [ctl_pin laser_control] 1 1]

# AXI Stream on XADC
set_property -dict [list CONFIG.ENABLE_AXI4STREAM {true}] [get_bd_cells xadc]
connect_pins xadc/s_axis_aclk adc_dac/adc_clk
connect_pins xadc/m_axis_tready [get_constant_pin 1 1]

# Start pid controller
set bd [current_bd_instance .]
current_bd_instance [create_bd_cell -type hier pid_controller]

create_bd_pin -dir I -from 15 -to 0 s_axis_tdata
create_bd_pin -dir I -from 15 -to 0 s_axis_tvalid
create_bd_pin -dir I -from 15 -to 0 setpoint
create_bd_pin -dir I -type clk clk
create_bd_pin -dir I integral_reset

create_bd_pin -dir O -from 12 -to 0 pid_control
create_bd_pin -dir O -from 15 -to 0 error

cell xilinx.com:ip:c_addsub:12.0 error {
  A_Type Signed
  B_Type Signed
  A_Width 16
  B_Width 16
  Add_Mode Subtract
} {
  A setpoint
  B s_axis_tdata
  CLK clk
  CE s_axis_tvalid
  S error
}

cell xilinx.com:ip:c_shift_ram:12.0 shift_reg {
  Depth 1
  Width 1
} {
  D s_axis_tvalid
  CLK clk
}

cell xilinx.com:ip:c_accum:12.0 integrator {
  Input_Width 16
  Output_Width 32
  CE true
} {
  B error/S
  CLK clk
  CE shift_reg/Q
  BYPASS integral_reset
}

cell koheron:user:saturation:1.0 saturation {
  DATA_WIDTH 24
  MAX_VAL 4095
} {
  clk clk
  din [get_slice_pin integrator/Q 31 8]
}

connect_pins pid_control [get_slice_pin saturation/dout 11 0]

current_bd_instance $bd
# End pid controller

connect_cell pid_controller {
  clk adc_dac/adc_clk
  s_axis_tdata xadc/m_axis_tdata
  s_axis_tvalid [get_and_pin xadc/m_axis_tvalid [get_EQ_pin xadc/m_axis_tid [get_constant_pin 17 5]] "clock_enable"]
  setpoint [ctl_pin power_setpoint]
  integral_reset [get_slice_pin [ctl_pin laser_control] 0 0]
}

cell koheron:user:bus_multiplexer:1.0 mux {
  WIDTH 12
} {
  din0 [ctl_pin laser_current]
  din1 pid_controller/pid_control
  sel [get_slice_pin [ctl_pin laser_control] 2 2]
  dout laser_current_pdm/din
  dout [sts_pin pid_control]
}

# Add address module
source $sdk_path/fpga/modules/address/address.tcl
set address_name address

address::create $address_name [expr [get_memory_addr_width dac0] + 1] [get_parameter n_dac]

connect_cell $address_name {
  clk  $adc_clk
  cfg  [ctl_pin addr]
}

# Add Dac controllers
source $sdk_path/fpga/lib/dac_controller.tcl

for {set i 0} {$i < [get_parameter n_dac]} {incr i} {

  connect_pins $address_name/period$i  [ctl_pin dac_period$i]

  set dac_controller$i dac${i}_ctrl
  add_single_dac_controller dac_controller$i dac$i [get_parameter dac_width] 1
  connect_cell dac_controller$i {
    clk  $adc_clk
    addr $address_name/addr$i
    rst  $rst_adc_clk_name/peripheral_reset
    dac  $adc_dac_name/dac[expr $i+1]
  }
}
