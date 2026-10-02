set display_name {AXI4-Stream Linear Feedback Shift-Register}

set core [ipx::current_core]

set_property DISPLAY_NAME $display_name $core
set_property DESCRIPTION $display_name $core

set_property VENDOR {pavel-demin} $core
set_property VENDOR_DISPLAY_NAME {Pavel Demin} $core
set_property COMPANY_URL {https://github.com/pavel-demin/red-pitaya-notes} $core

core_parameter AXIS_TDATA_WIDTH {AXIS TDATA WIDTH} {Width of the M_AXIS data bus.}
core_parameter SEED {Reset seed} {Initial 64-bit state. XOR feedback requires a nonzero seed.}
core_parameter FEEDBACK_MASK {Feedback taps} {Mask selecting the 64-bit feedback taps.}
core_parameter FEEDBACK_XNOR {Invert feedback} {Invert XOR feedback for the legacy XNOR recurrence.}

set bus [ipx::get_bus_interfaces -of_objects $core m_axis]
set_property NAME M_AXIS $bus
set_property INTERFACE_MODE master $bus

set bus [ipx::get_bus_interfaces aclk]
set parameter [ipx::get_bus_parameters -of_objects $bus ASSOCIATED_BUSIF]
set_property VALUE M_AXIS $parameter
