set core [ipx::current_core]
set_property VENDOR koheron $core
set_property DISPLAY_NAME {BIST shared converter and DMA datapath} $core
set_property DESCRIPTION {Digital loopback, DMA BRAM upload, dual DAC playback and ADC capture with atomic counters} $core
foreach bus {s_axis m_axis} {
    set intf [ipx::get_bus_interfaces -of_objects $core $bus]
    set_property NAME [string toupper $bus] $intf
}
set_property VALUE S_AXIS:M_AXIS [ipx::get_bus_parameters -of_objects [ipx::get_bus_interfaces aclk] ASSOCIATED_BUSIF]
