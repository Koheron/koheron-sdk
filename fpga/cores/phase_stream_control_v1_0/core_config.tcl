set core [ipx::current_core]
set_property DISPLAY_NAME {Continuous phase acquisition control} $core
set_property DESCRIPTION {Reset live phase, CIC, FIR and FIFO histories on acquisition changes; flag missed samples} $core
set_property VENDOR {koheron} $core
set_property VENDOR_DISPLAY_NAME {Koheron} $core
# Both resets belong to the ADC clock. Publishing this association lets block
# design validation distinguish the controlled reset from an asynchronous source.
set clock_bus [ipx::get_bus_interfaces aclk -of_objects $core]
set reset_parameter [ipx::get_bus_parameters ASSOCIATED_RESET -of_objects $clock_bus]
set_property VALUE {aresetn:filter_resetn} $reset_parameter
