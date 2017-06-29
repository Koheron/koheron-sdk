source $board_path/config/ports.tcl

# Add PS and AXI Interconnect
set board_preset $board_path/config/board_preset.tcl
source $sdk_path/fpga/lib/starting_point.tcl

# Add ADCs and DACs
source $sdk_path/fpga/lib/redp_adc_dac.tcl
set adc_dac_name adc_dac
add_redp_adc_dac $adc_dac_name

# Rename clocks
set adc_clk $adc_dac_name/adc_clk

# Add processor system reset synchronous to adc clock
set rst_adc_clk_name proc_sys_reset_adc_clk
cell xilinx.com:ip:proc_sys_reset:5.0 $rst_adc_clk_name {} {
  ext_reset_in ps_0/FCLK_RESET0_N
  slowest_sync_clk $adc_clk
}

# Add config and status registers
source $sdk_path/fpga/lib/ctl_sts.tcl
add_ctl_sts $adc_clk $rst_adc_clk_name/peripheral_aresetn

# Connect LEDs
connect_port_pin led_o [get_slice_pin [ctl_pin led] 7 0]

# Connect ADC to status register
for {set i 0} {$i < [get_parameter n_adc]} {incr i} {
  connect_pins [sts_pin adc$i] adc_dac/adc[expr $i + 1]
  connect_pins [ctl_pin dac$i] adc_dac/dac[expr $i + 1]
}

# Add XADC for laser current and laser power monitoring
source $sdk_path/fpga/lib/xadc.tcl
add_xadc xadc

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

cell xilinx.com:ip:c_shift_ram:12.0 c_shift_ram_0 {
  CE true
  Depth 1
} {
  D xadc/m_axis_tdata
  CLK adc_dac/adc_clk
  CE [get_and_pin xadc/m_axis_tvalid [get_EQ_pin xadc/m_axis_tid [get_constant_pin 17 5]] "clock_enable"]
  Q [sts_pin power]
}

# Start pid controller
set bd [current_bd_instance .]
current_bd_instance [create_bd_cell -type hier pid_controller]

create_bd_pin -dir I -from 15 -to 0 s_axis_tdata
create_bd_pin -dir I -from 15 -to 0 s_axis_tvalid
create_bd_pin -dir I -from 15 -to 0 setpoint
create_bd_pin -dir I -type clk clk
create_bd_pin -dir I integral_reset

create_bd_pin -dir O -from 12 -to 0 control
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
  SCLR true
} {
  B error/S
  CLK clk
  CE shift_reg/Q
  SCLR [get_slice_pin integrator/Q 31 31 "sign_bit"]
  BYPASS integral_reset
}

cell koheron:user:saturation:1.0 saturation {
  DATA_WIDTH 16
  MAX_VAL 4095
} {
  clk clk
  din [get_slice_pin integrator/Q 31 16]
}

connect_pins control [get_slice_pin saturation/dout 11 0]

current_bd_instance $bd
# End pid controller

connect_cell pid_controller {
  clk adc_dac/adc_clk
  s_axis_tdata xadc/m_axis_tdata
  s_axis_tvalid clock_enable/Res
  setpoint [ctl_pin power_setpoint]
  error [sts_pin error]
  integral_reset [get_slice_pin [ctl_pin laser_control] 0 0]
}

cell koheron:user:bus_multiplexer:1.0 mux {
  WIDTH 12
} {
  din0 [ctl_pin laser_current]
  din1 pid_controller/control
  sel [get_slice_pin [ctl_pin laser_control] 2 2]
  dout laser_current_pdm/din
  dout [sts_pin control]
}