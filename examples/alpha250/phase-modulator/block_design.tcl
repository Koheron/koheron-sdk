source $board_path/starting_point.tcl
source $sdk_path/fpga/ip/awg_v1_0/integration.tcl

# Each channel has its own 48-bit carrier and internal PM generator.
# Pass a dictionary of ENABLE_* switches to omit unused sources in an example.
for {set channel 0} {$channel < 2} {incr channel} {
    set output [dds_pm::add awg$channel awg$channel adc_dac/adc_clk \
        [get_parameter adc_clk] [dict create OUTPUT_WIDTH [get_parameter dac_width]]]
    connect_pins $output adc_dac/dac$channel
}

set_property STEPS.PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.PHYS_OPT_DESIGN.ARGS.DIRECTIVE AggressiveFanoutOpt [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.IS_ENABLED true [get_runs impl_1]
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.TCL.POST [file normalize [file join [file dirname [info script]] post_route.tcl]] [get_runs impl_1]
