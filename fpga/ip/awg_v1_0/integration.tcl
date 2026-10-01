# Shared configuration for simulation, synthesis and SDK block designs.
namespace eval dds_pm {
    variable controller_parameters {
        PHASE_WIDTH OUTPUT_WIDTH MOD_WIDTH LUT_BITS PRBS_WIDTH
        ENABLE_SINE ENABLE_SQUARE ENABLE_PULSE ENABLE_TRIANGLE ENABLE_UP_RAMP
        ENABLE_DOWN_RAMP ENABLE_UNIFORM ENABLE_GAUSSIAN ENABLE_PRBS ENABLE_BPSK
    }

    proc defaults {options} {
        variable controller_parameters
        set config [dict create PHASE_WIDTH 48 OUTPUT_WIDTH 16 MOD_WIDTH 24 LUT_BITS 14 PRBS_WIDTH 31]
        foreach key $controller_parameters {
            if {[string match ENABLE_* $key]} { dict set config $key 1 }
        }
        dict for {key value} $options {
            if {$key ni $controller_parameters} { error "Unknown DDS PM parameter: $key" }
            dict set config $key $value
        }
        foreach {key low high} {PHASE_WIDTH 32 48 OUTPUT_WIDTH 12 24 MOD_WIDTH 16 24 LUT_BITS 8 18} {
            set value [dict get $config $key]
            if {![string is integer -strict $value] || $value < $low || $value > $high} {
                error "$key must be an integer between $low and $high"
            }
        }
        if {[dict get $config PRBS_WIDTH] ni {7 15 23 31}} { error "PRBS_WIDTH must be 7, 15, 23 or 31" }
        foreach key $controller_parameters {
            if {[string match ENABLE_* $key] && [dict get $config $key] ni {0 1}} { error "$key must be 0 or 1" }
        }
        return $config
    }

    proc carrier_properties {config sample_rate} {
        return [list CONFIG.PartsPresent Phase_Generator_and_SIN_COS_LUT \
            CONFIG.DDS_Clock_Rate [expr {$sample_rate / 1000000.0}] \
            CONFIG.Parameter_Entry Hardware_Parameters \
            CONFIG.Phase_Width [dict get $config PHASE_WIDTH] \
            CONFIG.Output_Width [dict get $config OUTPUT_WIDTH] \
            CONFIG.Output_Selection Sine CONFIG.Phase_Increment Streaming \
            CONFIG.Phase_offset Streaming CONFIG.Resync true \
            CONFIG.Noise_Shaping Phase_Dithering CONFIG.Has_Phase_Out false \
            CONFIG.Has_ARESETn true CONFIG.Has_TREADY false \
            CONFIG.S_PHASE_Has_TUSER User_Field CONFIG.S_PHASE_TUSER_Width 1 \
            CONFIG.M_DATA_Has_TUSER User_Field]
    }

    proc modulation_properties {config sample_rate} {
        set tag_width [expr {3*[dict get $config PHASE_WIDTH]+[dict get $config MOD_WIDTH]+10}]
        return [list CONFIG.PartsPresent SIN_COS_LUT_only \
            CONFIG.DDS_Clock_Rate [expr {$sample_rate / 1000000.0}] \
            CONFIG.Parameter_Entry Hardware_Parameters \
            CONFIG.Phase_Width [dict get $config LUT_BITS] \
            CONFIG.Output_Width [dict get $config MOD_WIDTH] \
            CONFIG.Output_Selection Sine CONFIG.Noise_Shaping None \
            CONFIG.Has_Phase_Out false CONFIG.Has_ARESETn true CONFIG.Has_TREADY false \
            CONFIG.S_PHASE_Has_TUSER User_Field CONFIG.S_PHASE_TUSER_Width $tag_width \
            CONFIG.M_DATA_Has_TUSER User_Field]
    }

    # Instantiates one independent channel. Returns the signed DAC sample pin.
    # Call after the board starting_point, with this source in TCL_FILES.
    proc add {name memory sample_clk sample_rate {options {}} {interconnect 0}} {
        if {![string is double -strict $sample_rate] || $sample_rate <= 0 || $sample_rate > 250000000} {
            error "DDS PM sample clock must be positive and at most 250 MHz for the packaged CDC timing budget"
        }
        set config [defaults $options]
        set props {}
        dict for {key value} $config { lappend props $key $value }
        set axi_clock /[set ::ps_clk$interconnect]
        set axi_reset /[set ::rst${interconnect}_name]/peripheral_aresetn
        set slot [add_master_interface $interconnect]
        cell koheron:user:awg:1.0 $name $props [list \
            s_axi_aclk $axi_clock s_axi_aresetn $axi_reset sample_clk $sample_clk \
            S_AXI /axi_mem_intercon_$interconnect/M${slot}_AXI]
        set carrier ${name}_carrier
        create_bd_cell -type ip -vlnv xilinx.com:ip:dds_compiler:6.0 $carrier
        set_property -dict [carrier_properties $config $sample_rate] [get_bd_cells $carrier]
        connect_cell $carrier [list aclk $sample_clk aresetn $name/sample_resetn \
            S_AXIS_PHASE $name/M_AXIS_PHASE M_AXIS_DATA $name/S_AXIS_CARRIER]
        if {[dict get $config ENABLE_SINE]} {
            set modulation ${name}_modulation
            create_bd_cell -type ip -vlnv xilinx.com:ip:dds_compiler:6.0 $modulation
            set_property -dict [modulation_properties $config $sample_rate] [get_bd_cells $modulation]
            connect_cell $modulation [list aclk $sample_clk aresetn $name/sample_resetn \
                S_AXIS_PHASE $name/M_AXIS_MOD_PHASE M_AXIS_DATA $name/S_AXIS_MOD_DATA]
        } else {
            connect_cell $name [list \
                s_axis_mod_data_tdata [get_constant_pin 0 [expr {8*(([dict get $config MOD_WIDTH]+7)/8)}]] \
                s_axis_mod_data_tuser [get_constant_pin 0 [expr {3*[dict get $config PHASE_WIDTH]+[dict get $config MOD_WIDTH]+10}]] \
                s_axis_mod_data_tvalid [get_constant_pin 0 1]]
        }
        assign_bd_address -offset [get_memory_offset $memory] -range [get_memory_range $memory] \
            [get_bd_addr_segs $name/S_AXI/reg0]
        return $name/dac_data
    }
}
