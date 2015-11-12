set display_name {Pulse Width Modulator}

set core [ipx::current_core]

set_property DISPLAY_NAME $display_name $core
set_property DESCRIPTION $display_name $core

core_parameter PERIOD {PERIOD} {Period of the PWM}

