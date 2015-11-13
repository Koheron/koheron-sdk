set display_name {Pulse Width Modulator}

set core [ipx::current_core]

set_property DISPLAY_NAME $display_name $core
set_property DESCRIPTION $display_name $core

core_parameter NBITS {NBITS} {Number of bits of the PWM (period = 2^NBITS)}

