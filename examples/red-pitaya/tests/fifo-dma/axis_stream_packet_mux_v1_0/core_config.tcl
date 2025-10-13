set display_name {AXI Stream Packet Mux}

set core [ipx::current_core]

set_property DISPLAY_NAME $display_name $core
set_property DESCRIPTION $display_name $core

set_property VENDOR {koheron} $core
set_property VENDOR_DISPLAY_NAME {koheron} $core
set_property COMPANY_URL {www.koheron} $core

core_parameter DATA_WIDTH {DATA WIDTH} {}
core_parameter ADDR_WIDTH {ADDR WIDTH} {}

set bus [ipx::get_bus_interfaces -of_objects $core s_axi]
set_property NAME S_AXI_LITE $bus
set_property INTERFACE_MODE slave $bus

set bus [ipx::get_bus_interfaces aclk]
set parameter [ipx::get_bus_parameters -of_objects $bus ASSOCIATED_BUSIF]
set_property VALUE S_AXI_LITE $parameter

set bus [ipx::get_bus_interfaces -of_objects $core s_axis_0]
set_property NAME S_AXIS_0 $bus
set_property INTERFACE_MODE slave $bus

set bus [ipx::get_bus_interfaces -of_objects $core s_axis_1]
set_property NAME S_AXIS_1 $bus
set_property INTERFACE_MODE slave $bus

set bus [ipx::get_bus_interfaces -of_objects $core m_axis]
set_property NAME M_AXIS $bus
set_property INTERFACE_MODE master $bus

