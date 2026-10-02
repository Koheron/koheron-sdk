set core [ipx::current_core]
set_property DISPLAY_NAME {Phase extraction prefilter} $core
set_property DESCRIPTION {Four full-precision 16-sample moving sums with stochastic output rounding} $core
set_property VENDOR {koheron} $core
set_property VENDOR_DISPLAY_NAME {Koheron} $core
core_parameter OUTPUT_WIDTH {Output width} {16 to 24 bits; extra output bits retain fractional I/Q counts}
