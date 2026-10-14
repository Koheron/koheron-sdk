set core [ipx::current_core]
set_property VENDOR koheron $core
set_property DISPLAY_NAME {BIST FIFO UART} $core
set_property DESCRIPTION {AXI-Lite full duplex 8N1 UART with fractional baud clock and diagnostics} $core
foreach p {CLOCK_HZ BAUD FIFO_BITS} {core_parameter $p $p $p}
set bus [ipx::get_bus_interfaces -of_objects $core s_axi]
set_property NAME S_AXI $bus
set_property INTERFACE_MODE slave $bus
set_property VALUE S_AXI [ipx::get_bus_parameters -of_objects [ipx::get_bus_interfaces aclk] ASSOCIATED_BUSIF]
