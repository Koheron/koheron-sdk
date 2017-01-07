set display_name {Comparator}

set core [ipx::current_core]

set_property DISPLAY_NAME $display_name $core
set_property DESCRIPTION $display_name $core

set_property VENDOR {koheron} $core
set_property VENDOR_DISPLAY_NAME {Koheron} $core
set_property COMPANY_URL {http://www.koheron.com} $core

set_property value_validation_list {GE GT LE LT EQ NE} [ipx::get_user_parameters OPERATION -of_objects $core]
