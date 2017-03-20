set display_name {Register on ce pulse}

set core [ipx::current_core]

set_property DISPLAY_NAME $display_name $core
set_property DESCRIPTION $display_name $core

set_property VENDOR {CCFE} $core
set_property VENDOR_DISPLAY_NAME {CCFE} $core
set_property COMPANY_URL {http://www.ccfe.ac.uk} $core
