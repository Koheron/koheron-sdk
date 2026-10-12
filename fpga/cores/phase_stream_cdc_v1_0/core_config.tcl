set core [ipx::current_core]
set_property DISPLAY_NAME {Phase acquisition clock crossing} $core
set_property DESCRIPTION {Epoch-stable configuration, synchronized reset and metadata, coherent reverse packet status} $core
set_property VENDOR {koheron} $core
set_property VENDOR_DISPLAY_NAME {Koheron} $core
# The output reset is synchronized to clk, not the reverse-status clock.
set clock_bus [ipx::get_bus_interfaces clk -of_objects $core]
set reset_parameter [ipx::get_bus_parameters ASSOCIATED_RESET -of_objects $clock_bus]
if {![llength $reset_parameter]} {
    set reset_parameter [ipx::add_bus_parameter ASSOCIATED_RESET $clock_bus]
}
set_property VALUE resetn $reset_parameter
