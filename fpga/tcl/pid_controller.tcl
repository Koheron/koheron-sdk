
proc add_pid_module {module_name width acc_width} {

  set bd [current_bd_instance .]
  current_bd_instance [create_bd_cell -type hier $module_name]

  create_bd_pin -dir I -type clk clk
  create_bd_pin -dir I -from $width -to 0 error_in
  create_bd_pin -dir I integral_reset
  foreach {name} {p i d} {
    create_bd_pin -dir I -from $width -to 0 coef_$name
  }

  create_bd_pin -dir O -from $width -to 0 cmd_out

  foreach {name} {p i d} {
    cell xilinx.com:ip:mult_gen:12.0 mult_$name {
      PipeStages 2 
      Use_Custom_Output_Width true
      PortAWidth $width
      PortBWidth $width
      OutputWidthHigh [expr 2*$width - 1]
    } {
      clk clk
      A error_in
      B coef_$name
    }
  }
  set_cell_props mult_p {PipeStages 3}

  # Integral part
  cell xilinx.com:ip:c_accum:12.0 accumulator {
    Latency 1
    Input_Width [expr 2*$width]
    Output_Width $acc_width
  } {
    clk clk
    BYPASS integral_reset
    B mult_i/P
  }

  # Derivative part
  cell xilinx.com:ip:c_addsub:12.0 deriv_substract {
    CE false
    Add_Mode Subtract
  } {
    clk clk
    A mult_d/P
    B [get_Q_pin mult_d/P]
  }

  # Add proportional with integral part
  cell xilinx.com:ip:c_addsub:12.0 p_plus_i {
    CE false
  } {
    clk clk
    A mult_p/P
    B accumulator/Q
  }

  # Add derivative part
  cell xilinx.com:ip:c_addsub:12.0 pi_plus_d {
    CE false
  } {
    clk clk
    A p_plus_i/S
    B deriv_substract/S
    S cmd_out
  }

  current_bd_instance $bd
} 
