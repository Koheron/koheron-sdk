set core [ipx::current_core]
set_property DISPLAY_NAME {AXI4-Stream Float Accumulator} $core
set_property TAXONOMY /DSP/Math_Functions $core
set_property DESCRIPTION {Double-buffered float32 frame sums with backpressure and frame validation} $core
set_property VENDOR koheron $core
set_property VENDOR_DISPLAY_NAME Koheron $core
set_property COMPANY_URL {https://www.koheron.com} $core
# Ship the usage guide inside the catalog package and exported ZIP.
file copy -force $core_path/README.md $out_dir/README.md
set guide [ipx::add_file_group -type xilinx_productguide xilinx_productguide $core]
ipx::add_file README.md $guide
foreach {name title description} {
    FRAME_LENGTH {Bins per frame} {Number of float32 bins between TLAST markers.}
    N_FRAMES {Frames per sum} {Number of input frames accumulated into one output frame.}
} {
    core_parameter $name $title $description
    set p [ipx::get_user_parameters $name -of_objects $core]
    set_property value_validation_type range_long $p
    set_property value_validation_range_minimum 1 $p
    set_property value_validation_range_maximum 65536 $p
}
core_parameter CHECK_TLAST {Validate TLAST} {Check input frame boundaries; disable only for fixed-length legacy streams without TLAST.}
set p [ipx::get_user_parameters CHECK_TLAST -of_objects $core]
set_property value_validation_type list $p
set_property value_validation_list {0 1} $p
core_parameter SYNC_ON_RESET {Synchronize after reset} {Discard through the first TLAST after reset when the upstream pipeline keeps running. Requires CHECK_TLAST=1.}
set p [ipx::get_user_parameters SYNC_ON_RESET -of_objects $core]
set_property value_validation_type list $p
set_property value_validation_list {0 1} $p
foreach {old new} {s_axis S_AXIS m_axis M_AXIS} {
    set bus [ipx::get_bus_interfaces $old -of_objects $core]
    set_property NAME $new $bus
}
set bus [ipx::get_bus_interfaces aclk -of_objects $core]
foreach {name value} {ASSOCIATED_BUSIF S_AXIS:M_AXIS ASSOCIATED_RESET aresetn} {
    set p [ipx::get_bus_parameters $name -of_objects $bus]
    if {![llength $p]} {set p [ipx::add_bus_parameter $name $bus]}
    set_property VALUE $value $p
}
