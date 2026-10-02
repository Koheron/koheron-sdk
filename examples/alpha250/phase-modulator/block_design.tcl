source $board_path/starting_point.tcl
source $sdk_path/fpga/ip/awg_v1_0/integration.tcl

# A single catalog IP contains both controllers and all four vendor DDS cores.
set outputs [dds_pm::add awg awg adc_dac/adc_clk [get_parameter adc_clk] \
    [dict create CHANNELS 2 OUTPUT_WIDTH [get_parameter dac_width]]]
for {set channel 0} {$channel < [llength $outputs]} {incr channel} {
    connect_pins [lindex $outputs $channel] adc_dac/dac$channel
}

set_property STEPS.PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.PHYS_OPT_DESIGN.ARGS.DIRECTIVE AggressiveFanoutOpt [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.TCL.POST [file normalize [file join [file dirname [info script]] post_route.tcl]] [get_runs impl_1]
