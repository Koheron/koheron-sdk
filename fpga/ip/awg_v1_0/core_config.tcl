set core [ipx::current_core]
set_property DISPLAY_NAME {AXI DDS phase modulator} $core
set_property DESCRIPTION {One or two complete DDS phase-modulator channels with embedded Xilinx DDS cores} $core
set_property VENDOR koheron $core
set_property VENDOR_DISPLAY_NAME Koheron $core
set_property COMPANY_URL {https://www.koheron.com} $core

foreach {name title description minimum maximum} {
    PHASE_WIDTH {DDS phase width} {Native carrier and modulation phase precision in bits.} 32 48
    OUTPUT_WIDTH {Carrier sample width} {Signed sample width of the carrier DDS and DAC output.} 12 24
    MOD_WIDTH {Modulation sample width} {Modulation amplitude precision, independently of phase precision.} 16 24
    LUT_BITS {Modulation sine address width} {Internal sine table phase address width.} 8 18
    AXI_ADDR_WIDTH {AXI address width} {Fixed 13-bit address for two 4 KiB channel banks.} 13 13
} {
    core_parameter $name $title $description
    set p [ipx::get_user_parameters $name -of_objects $core]
    set_property value_validation_type range_long $p
    set_property value_validation_range_minimum $minimum $p
    set_property value_validation_range_maximum $maximum $p
}
# Configured vendor DDS widths cannot change through parent HDL generics.
# Regenerate the package with package_settings.tcl for another precision profile.
source $core_path/package_settings.tcl
dict for {name value} $dds_pm_package_settings {
    foreach p [concat [ipx::get_user_parameters $name -of_objects $core] [ipx::get_hdl_parameters $name -of_objects $core]] {
        set_property value $value $p
    }
    set p [ipx::get_user_parameters $name -of_objects $core]
    set_property value_validation_type list $p
    set_property value_validation_list [list $value] $p
}
core_parameter CHANNELS {Output channels} {Number of independent DDS phase-modulator channels (1 or 2).}
set p [ipx::get_user_parameters CHANNELS -of_objects $core]
set_property value_validation_type list $p
set_property value_validation_list {1 2} $p
# Only the second DAC port is present on two-channel instances.
set_property enablement_dependency {spirit:decode(id('PARAM_VALUE.CHANNELS')) = 2} [ipx::get_ports dac1_data -of_objects $core]
core_parameter PRBS_WIDTH {PRBS order} {Maximal-length PRBS polynomial: PN7, PN15, PN23 or PN31.}
set p [ipx::get_user_parameters PRBS_WIDTH -of_objects $core]
set_property value_validation_type list $p
set_property value_validation_list {7 15 23 31} $p

foreach {name title} {
    ENABLE_SINE Sine ENABLE_SQUARE Square ENABLE_PULSE Pulse
    ENABLE_TRIANGLE Triangle ENABLE_UP_RAMP {Up ramp} ENABLE_DOWN_RAMP {Down ramp}
    ENABLE_UNIFORM {Uniform noise} ENABLE_GAUSSIAN {Approximate Gaussian noise}
    ENABLE_PRBS PRBS ENABLE_BPSK {Binary phase shift keying}
} {
    core_parameter $name "Include $title" "Include $title modulation hardware in this build."
    set p [ipx::get_user_parameters $name -of_objects $core]
    set_property value_validation_type list $p
    set_property value_validation_list {0 1} $p
}

# Normalize inferred interface names and clock associations.
set bus [ipx::get_bus_interfaces s_axi -of_objects $core]
if {[llength $bus]} {set_property NAME S_AXI $bus}
set bus [ipx::get_bus_interfaces s_axi_aclk -of_objects $core]
foreach {name value} {ASSOCIATED_BUSIF S_AXI ASSOCIATED_RESET s_axi_aresetn} {
    set p [ipx::get_bus_parameters $name -of_objects $bus]
    if {![llength $p]} {set p [ipx::add_bus_parameter $name $bus]}
    set_property VALUE $value $p
}
# There are no stream interfaces outside the integrated subsystem.
set bus [ipx::get_bus_interfaces sample_clk -of_objects $core]
set p [ipx::get_bus_parameters ASSOCIATED_BUSIF -of_objects $bus]
if {[llength $p]} {ipx::remove_bus_parameter ASSOCIATED_BUSIF $bus}
# AXI decoding always reserves both banks, including single-channel instances.
set map [ipx::get_memory_maps s_axi -of_objects $core]
set block [ipx::get_address_blocks reg0 -of_objects $map]
set_property range 8192 $block

# Carry the mailbox timing constraints with the packaged IP.
set group [ipx::get_file_groups xilinx_implementation -of_objects $core]
if {![llength $group]} { set group [ipx::add_file_group -type xilinx_implementation xilinx_implementation $core] }
file copy -force $core_path/awg_cdc.xdc $out_dir/awg_cdc.xdc
set xdc [ipx::add_file awg_cdc.xdc $group]
set_property type xdc $xdc
set_property scoped_to_ref awg_control $xdc
set_property processing_order LATE $xdc
