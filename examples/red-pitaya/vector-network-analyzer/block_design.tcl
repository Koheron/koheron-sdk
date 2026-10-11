source $board_path/config/ports.tcl
source $board_path/base_system.tcl
connect_port_pin dac_pwm_o [get_constant_pin 0 4]
connect_cell adc_dac { adc1 [sts_pin adc0] adc2 [sts_pin adc1] }
# Normalize signed 14-bit ADC samples to the shared signed 16-bit DSP scale.
set adc [get_concat_pin [list [get_constant_pin 0 2] adc_dac/adc1]]
source $sdk_path/fpga/modules/vector-network-analyzer.tcl
vector_network_analyzer $adc adc_dac/adc_clk proc_sys_reset_adc_clk/peripheral_aresetn
connect_pins adc_dac/dac1 [get_slice_pin capture/dac 15 2]
connect_pins adc_dac/dac2 [get_constant_pin 0 14]
validate_bd_design
# Optimize the PS DMA interconnect's routed setup paths, then repair hold paths
# and refresh the reports used by the SDK's strict timing gate.
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.ARGS.DIRECTIVE Explore [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.TCL.POST [file normalize $sdk_path/fpga/lib/post_route_hold_fix.tcl] [get_runs impl_1]
