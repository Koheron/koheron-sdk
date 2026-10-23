set core [ipx::current_core]
set_property VENDOR koheron $core
set_property DISPLAY_NAME {Fenced independent DDR BIST master} $core
set_property DESCRIPTION {64-bit AXI burst writer/checker, byte strobes, pause injection, first-failure diagnostics and draining abort} $core
set intf [ipx::get_bus_interfaces -of_objects $core m_axi]
set_property NAME M_AXI $intf
set_property VALUE M_AXI [ipx::get_bus_parameters -of_objects [ipx::get_bus_interfaces aclk] ASSOCIATED_BUSIF]
