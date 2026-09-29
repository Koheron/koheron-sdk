set root [file normalize [file join [file dirname [info script]] ../../..]]
set out [file normalize [lindex $argv 0]]
create_project -force psd_integration $out -part xc7z020clg400-2
add_files $root/fpga/cores/psd_counter_v1_0/psd_counter.v
create_bd_design system
source $root/fpga/lib/utilities.tcl
source $root/fpga/modules/bram_accumulator/bram_accumulator.tcl
create_bd_port -dir I -type clk clk
set_property CONFIG.FREQ_HZ 250000000 [get_bd_ports clk]
create_bd_port -dir I -from 31 -to 0 data
create_bd_port -dir I valid
create_bd_port -dir O -from 31 -to 0 sum
create_bd_port -dir O -from 31 -to 0 sum_addr
create_bd_port -dir O -from 3 -to 0 wen
create_bd_cell -type module -reference psd_counter counter
set_property -dict {CONFIG.PERIOD 64 CONFIG.PERIOD_WIDTH 6 CONFIG.N_CYCLES 3 CONFIG.N_CYCLES_WIDTH 2} [get_bd_cells counter]
bram_accumulator::create accum
connect_port_pin clk counter/clk
connect_port_pin clk accum/clk
connect_port_pin data counter/s_axis_tdata
connect_port_pin valid counter/s_axis_tvalid
connect_cell accum {
 s_axis_tdata counter/m_axis_tdata
 s_axis_tvalid counter/m_axis_tvalid
 addr_in [get_concat_pin [list counter/addr [get_constant_pin 0 24]]]
 first_cycle counter/first_cycle
 last_cycle counter/last_cycle
}
connect_port_pin sum accum/m_axis_tdata
connect_port_pin sum_addr accum/addr_out
connect_port_pin wen accum/wen
validate_bd_design
save_bd_design
set bd [get_files system.bd]
generate_target all $bd
set wrapper [make_wrapper -files $bd -top]
add_files $wrapper
add_files -fileset sim_1 $root/fpga/tests/psd_accumulator/test_bench.sv
set_property top integration_tb [get_filesets sim_1]
set_property xsim.simulate.runtime 0ns [get_filesets sim_1]
update_compile_order -fileset sources_1
update_compile_order -fileset sim_1
foreach pauses {0 1} {
  set_property generic PAUSES=$pauses [get_filesets sim_1]
  launch_simulation
  run all
  set passed [get_value -radix bin [get_objects /*/passed]]
  close_sim
  if {$passed ne "1"} {error "PSD accumulator regression failed (pauses=$pauses)"}
}
