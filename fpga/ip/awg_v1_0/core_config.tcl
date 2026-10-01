set core [ipx::current_core]
set_property DISPLAY_NAME {AXI DDS phase modulator} $core
set_property DESCRIPTION {48-bit DDS control with configurable internal phase modulation sources} $core
set_property VENDOR koheron $core
set_property VENDOR_DISPLAY_NAME Koheron $core
set_property COMPANY_URL {https://www.koheron.com} $core

foreach {name title description minimum maximum} {
    PHASE_WIDTH {DDS phase width} {Native carrier and modulation phase precision in bits.} 32 48
    OUTPUT_WIDTH {Carrier sample width} {Signed sample width of the carrier DDS and DAC output.} 12 24
    MOD_WIDTH {Modulation sample width} {Modulation amplitude precision, independently of phase precision.} 16 24
    LUT_BITS {Modulation sine address width} {Internal sine table phase address width.} 8 18
    AXI_ADDR_WIDTH {AXI address width} {Fixed 12-bit address for the 4 KiB register window.} 12 12
} {
    core_parameter $name $title $description
    set p [ipx::get_user_parameters $name -of_objects $core]
    set_property value_validation_type range_long $p
    set_property value_validation_range_minimum $minimum $p
    set_property value_validation_range_maximum $maximum $p
}
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
foreach {old new} {
    s_axi S_AXI m_axis_phase M_AXIS_PHASE
    m_axis_mod_phase M_AXIS_MOD_PHASE s_axis_mod_data S_AXIS_MOD_DATA
    s_axis_carrier S_AXIS_CARRIER
} {
    set bus [ipx::get_bus_interfaces $old -of_objects $core]
    if {[llength $bus]} { set_property NAME $new $bus }
}
foreach {clock buses reset} {
    s_axi_aclk S_AXI s_axi_aresetn
    sample_clk {M_AXIS_PHASE:M_AXIS_MOD_PHASE:S_AXIS_MOD_DATA:S_AXIS_CARRIER} sample_resetn
} {
    set bus [ipx::get_bus_interfaces $clock -of_objects $core]
    foreach {name value} [list ASSOCIATED_BUSIF $buses ASSOCIATED_RESET $reset] {
        set p [ipx::get_bus_parameters $name -of_objects $bus]
        if {![llength $p]} { set p [ipx::add_bus_parameter $name $bus] }
        set_property VALUE $value $p
    }
}

# Carry the mailbox timing constraints with the packaged IP.
set group [ipx::get_file_groups xilinx_implementation -of_objects $core]
if {![llength $group]} { set group [ipx::add_file_group -type xilinx_implementation xilinx_implementation $core] }
file copy -force $core_path/awg_cdc.xdc $out_dir/awg_cdc.xdc
set xdc [ipx::add_file awg_cdc.xdc $group]
set_property type xdc $xdc
set_property scoped_to_ref awg $xdc
set_property processing_order LATE $xdc
