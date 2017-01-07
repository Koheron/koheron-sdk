set display_name {Pulse Density Modulator}

set core [ipx::current_core]

set_property DISPLAY_NAME $display_name $core
set_property DESCRIPTION $display_name $core

set_property VENDOR {koheron} $core
set_property VENDOR_DISPLAY_NAME {Koheron} $core
set_property COMPANY_URL {http://www.koheron.com} $core

core_parameter NBITS {NBITS} {Number of bits of the PDM (period = 2^(NBITS-1))}
