set core [ipx::current_core]
set_property DISPLAY_NAME {MMCM DRP mailbox} $core
set_property DESCRIPTION {MMCM DRP access from the independent PS clock domain} $core
set_property VENDOR {koheron} $core
set_property VENDOR_DISPLAY_NAME {Koheron} $core
set_property COMPANY_URL {http://www.koheron.com} $core
set_property VALUE ACTIVE_HIGH [ipx::add_bus_parameter POLARITY [ipx::get_bus_interfaces reset -of_objects $core]]
ipx::remove_bus_parameter ASSOCIATED_RESET [ipx::get_bus_interfaces aclk -of_objects $core]
