set display_name {Boxcar filter}

set core [ipx::current_core]

set_property DISPLAY_NAME $display_name $core
set_property DESCRIPTION $display_name $core

set_property VENDOR {koheron} $core
set_property VENDOR_DISPLAY_NAME {Koheron} $core
set_property COMPANY_URL {http://www.koheron.com} $core

core_parameter LOW_LATENCY {Low latency} {Use the current input in the partial sums to save one clock cycle. Disabled by default to preserve the existing pipeline delay.}
