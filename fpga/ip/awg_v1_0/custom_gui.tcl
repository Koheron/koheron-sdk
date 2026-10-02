# Vivado customization UI. All controls map to synthesis parameters.
# IP validation callbacks require the literal strings true/false, not Tcl 1/0.
proc init_gui {IPINST} {
    ipgui::add_param $IPINST -name Component_Name
    set widget [ipgui::add_param $IPINST -name CHANNELS -widget comboBox]
    set_property display_name {Output channels (1/2)} $widget
    set precision [ipgui::add_page $IPINST -name Precision]
    foreach {name label tooltip} {
        PHASE_WIDTH {DDS phase width} {Carrier and modulation phase accumulators; default 48 bits.}
        OUTPUT_WIDTH {Carrier sample width} {Signed carrier output width; independent of phase precision.}
        MOD_WIDTH {Modulation sample width} {Internal PM amplitude precision, from 16 to 24 bits.}
        LUT_BITS {Modulation sine address bits} {Size of the internal sine lookup table, from 8 to 18 address bits.}
    } {
        set widget [ipgui::add_param $IPINST -name $name -parent $precision]
        set_property display_name $label $widget
        set_property tooltip "$tooltip Configured when generating the package." $widget
        set_property enabled false $widget
    }
    set sources [ipgui::add_page $IPINST -name {Internal PM sources}]
    foreach {name label} {
        ENABLE_SINE Sine ENABLE_SQUARE Square ENABLE_PULSE Pulse ENABLE_TRIANGLE Triangle
        ENABLE_UP_RAMP {Up ramp} ENABLE_DOWN_RAMP {Down ramp}
        ENABLE_UNIFORM {Uniform noise} ENABLE_GAUSSIAN {Approximate Gaussian noise}
        ENABLE_PRBS PRBS ENABLE_BPSK BPSK
    } {
        set widget [ipgui::add_param $IPINST -name $name -parent $sources -widget comboBox]
        set_property display_name "Include $label (0/1)" $widget
        set_property tooltip "Set 0 to remove $label hardware during synthesis." $widget
    }
    set widget [ipgui::add_param $IPINST -name PRBS_WIDTH -parent $sources -widget comboBox]
    set_property display_name {PRBS order} $widget
    set interface [ipgui::add_page $IPINST -name Interface]
    set widget [ipgui::add_param $IPINST -name AXI_ADDR_WIDTH -parent $interface]
    set_property display_name {AXI address width} $widget
    set_property enabled false $widget
}

proc validate_PARAM_VALUE.PHASE_WIDTH {PARAM_VALUE.PHASE_WIDTH} {
    set value [get_property value ${PARAM_VALUE.PHASE_WIDTH}]
    if {![string is integer -strict $value]} { return false }
    if {$value < 32 || $value > 48} { return false }
    return true
}
proc update_PARAM_VALUE.PHASE_WIDTH {PARAM_VALUE.PHASE_WIDTH} {
}
proc update_MODELPARAM_VALUE.PHASE_WIDTH {MODELPARAM_VALUE.PHASE_WIDTH PARAM_VALUE.PHASE_WIDTH} {
    set_property value [get_property value ${PARAM_VALUE.PHASE_WIDTH}] ${MODELPARAM_VALUE.PHASE_WIDTH}
}

proc validate_PARAM_VALUE.OUTPUT_WIDTH {PARAM_VALUE.OUTPUT_WIDTH} {
    set value [get_property value ${PARAM_VALUE.OUTPUT_WIDTH}]
    if {![string is integer -strict $value]} { return false }
    if {$value < 12 || $value > 24} { return false }
    return true
}
proc update_PARAM_VALUE.OUTPUT_WIDTH {PARAM_VALUE.OUTPUT_WIDTH} {
}
proc update_MODELPARAM_VALUE.OUTPUT_WIDTH {MODELPARAM_VALUE.OUTPUT_WIDTH PARAM_VALUE.OUTPUT_WIDTH} {
    set_property value [get_property value ${PARAM_VALUE.OUTPUT_WIDTH}] ${MODELPARAM_VALUE.OUTPUT_WIDTH}
}

proc validate_PARAM_VALUE.MOD_WIDTH {PARAM_VALUE.MOD_WIDTH} {
    set value [get_property value ${PARAM_VALUE.MOD_WIDTH}]
    if {![string is integer -strict $value]} { return false }
    if {$value < 16 || $value > 24} { return false }
    return true
}
proc update_PARAM_VALUE.MOD_WIDTH {PARAM_VALUE.MOD_WIDTH} {
}
proc update_MODELPARAM_VALUE.MOD_WIDTH {MODELPARAM_VALUE.MOD_WIDTH PARAM_VALUE.MOD_WIDTH} {
    set_property value [get_property value ${PARAM_VALUE.MOD_WIDTH}] ${MODELPARAM_VALUE.MOD_WIDTH}
}

proc validate_PARAM_VALUE.LUT_BITS {PARAM_VALUE.LUT_BITS} {
    set value [get_property value ${PARAM_VALUE.LUT_BITS}]
    if {![string is integer -strict $value]} { return false }
    if {$value < 8 || $value > 18} { return false }
    return true
}
proc update_PARAM_VALUE.LUT_BITS {PARAM_VALUE.LUT_BITS PARAM_VALUE.ENABLE_SINE} {
    set_property enabled false ${PARAM_VALUE.LUT_BITS}
}
proc update_MODELPARAM_VALUE.LUT_BITS {MODELPARAM_VALUE.LUT_BITS PARAM_VALUE.LUT_BITS} {
    set_property value [get_property value ${PARAM_VALUE.LUT_BITS}] ${MODELPARAM_VALUE.LUT_BITS}
}

proc validate_PARAM_VALUE.AXI_ADDR_WIDTH {PARAM_VALUE.AXI_ADDR_WIDTH} {
    set value [get_property value ${PARAM_VALUE.AXI_ADDR_WIDTH}]
    if {![string is integer -strict $value]} { return false }
    if {$value < 13 || $value > 13} { return false }
    return true
}
proc update_PARAM_VALUE.AXI_ADDR_WIDTH {PARAM_VALUE.AXI_ADDR_WIDTH} {
}
proc update_MODELPARAM_VALUE.AXI_ADDR_WIDTH {MODELPARAM_VALUE.AXI_ADDR_WIDTH PARAM_VALUE.AXI_ADDR_WIDTH} {
    set_property value [get_property value ${PARAM_VALUE.AXI_ADDR_WIDTH}] ${MODELPARAM_VALUE.AXI_ADDR_WIDTH}
}

proc validate_PARAM_VALUE.PRBS_WIDTH {PARAM_VALUE.PRBS_WIDTH} {
    if {[get_property value ${PARAM_VALUE.PRBS_WIDTH}] ni {7 15 23 31}} { return false }
    return true
}
proc update_PARAM_VALUE.PRBS_WIDTH {PARAM_VALUE.PRBS_WIDTH PARAM_VALUE.ENABLE_PRBS} {
    set_property enabled [get_property value ${PARAM_VALUE.ENABLE_PRBS}] ${PARAM_VALUE.PRBS_WIDTH}
}
proc update_MODELPARAM_VALUE.PRBS_WIDTH {MODELPARAM_VALUE.PRBS_WIDTH PARAM_VALUE.PRBS_WIDTH} {
    set_property value [get_property value ${PARAM_VALUE.PRBS_WIDTH}] ${MODELPARAM_VALUE.PRBS_WIDTH}
}

proc validate_PARAM_VALUE.ENABLE_SINE {PARAM_VALUE.ENABLE_SINE} {
    if {[get_property value ${PARAM_VALUE.ENABLE_SINE}] ni {0 1}} { return false }
    return true
}
proc update_PARAM_VALUE.ENABLE_SINE {PARAM_VALUE.ENABLE_SINE} {
}
proc update_MODELPARAM_VALUE.ENABLE_SINE {MODELPARAM_VALUE.ENABLE_SINE PARAM_VALUE.ENABLE_SINE} {
    set_property value [get_property value ${PARAM_VALUE.ENABLE_SINE}] ${MODELPARAM_VALUE.ENABLE_SINE}
}

proc validate_PARAM_VALUE.ENABLE_SQUARE {PARAM_VALUE.ENABLE_SQUARE} {
    if {[get_property value ${PARAM_VALUE.ENABLE_SQUARE}] ni {0 1}} { return false }
    return true
}
proc update_PARAM_VALUE.ENABLE_SQUARE {PARAM_VALUE.ENABLE_SQUARE} {
}
proc update_MODELPARAM_VALUE.ENABLE_SQUARE {MODELPARAM_VALUE.ENABLE_SQUARE PARAM_VALUE.ENABLE_SQUARE} {
    set_property value [get_property value ${PARAM_VALUE.ENABLE_SQUARE}] ${MODELPARAM_VALUE.ENABLE_SQUARE}
}

proc validate_PARAM_VALUE.ENABLE_PULSE {PARAM_VALUE.ENABLE_PULSE} {
    if {[get_property value ${PARAM_VALUE.ENABLE_PULSE}] ni {0 1}} { return false }
    return true
}
proc update_PARAM_VALUE.ENABLE_PULSE {PARAM_VALUE.ENABLE_PULSE} {
}
proc update_MODELPARAM_VALUE.ENABLE_PULSE {MODELPARAM_VALUE.ENABLE_PULSE PARAM_VALUE.ENABLE_PULSE} {
    set_property value [get_property value ${PARAM_VALUE.ENABLE_PULSE}] ${MODELPARAM_VALUE.ENABLE_PULSE}
}

proc validate_PARAM_VALUE.ENABLE_TRIANGLE {PARAM_VALUE.ENABLE_TRIANGLE} {
    if {[get_property value ${PARAM_VALUE.ENABLE_TRIANGLE}] ni {0 1}} { return false }
    return true
}
proc update_PARAM_VALUE.ENABLE_TRIANGLE {PARAM_VALUE.ENABLE_TRIANGLE} {
}
proc update_MODELPARAM_VALUE.ENABLE_TRIANGLE {MODELPARAM_VALUE.ENABLE_TRIANGLE PARAM_VALUE.ENABLE_TRIANGLE} {
    set_property value [get_property value ${PARAM_VALUE.ENABLE_TRIANGLE}] ${MODELPARAM_VALUE.ENABLE_TRIANGLE}
}

proc validate_PARAM_VALUE.ENABLE_UP_RAMP {PARAM_VALUE.ENABLE_UP_RAMP} {
    if {[get_property value ${PARAM_VALUE.ENABLE_UP_RAMP}] ni {0 1}} { return false }
    return true
}
proc update_PARAM_VALUE.ENABLE_UP_RAMP {PARAM_VALUE.ENABLE_UP_RAMP} {
}
proc update_MODELPARAM_VALUE.ENABLE_UP_RAMP {MODELPARAM_VALUE.ENABLE_UP_RAMP PARAM_VALUE.ENABLE_UP_RAMP} {
    set_property value [get_property value ${PARAM_VALUE.ENABLE_UP_RAMP}] ${MODELPARAM_VALUE.ENABLE_UP_RAMP}
}

proc validate_PARAM_VALUE.ENABLE_DOWN_RAMP {PARAM_VALUE.ENABLE_DOWN_RAMP} {
    if {[get_property value ${PARAM_VALUE.ENABLE_DOWN_RAMP}] ni {0 1}} { return false }
    return true
}
proc update_PARAM_VALUE.ENABLE_DOWN_RAMP {PARAM_VALUE.ENABLE_DOWN_RAMP} {
}
proc update_MODELPARAM_VALUE.ENABLE_DOWN_RAMP {MODELPARAM_VALUE.ENABLE_DOWN_RAMP PARAM_VALUE.ENABLE_DOWN_RAMP} {
    set_property value [get_property value ${PARAM_VALUE.ENABLE_DOWN_RAMP}] ${MODELPARAM_VALUE.ENABLE_DOWN_RAMP}
}

proc validate_PARAM_VALUE.ENABLE_UNIFORM {PARAM_VALUE.ENABLE_UNIFORM} {
    if {[get_property value ${PARAM_VALUE.ENABLE_UNIFORM}] ni {0 1}} { return false }
    return true
}
proc update_PARAM_VALUE.ENABLE_UNIFORM {PARAM_VALUE.ENABLE_UNIFORM} {
}
proc update_MODELPARAM_VALUE.ENABLE_UNIFORM {MODELPARAM_VALUE.ENABLE_UNIFORM PARAM_VALUE.ENABLE_UNIFORM} {
    set_property value [get_property value ${PARAM_VALUE.ENABLE_UNIFORM}] ${MODELPARAM_VALUE.ENABLE_UNIFORM}
}

proc validate_PARAM_VALUE.ENABLE_GAUSSIAN {PARAM_VALUE.ENABLE_GAUSSIAN} {
    if {[get_property value ${PARAM_VALUE.ENABLE_GAUSSIAN}] ni {0 1}} { return false }
    return true
}
proc update_PARAM_VALUE.ENABLE_GAUSSIAN {PARAM_VALUE.ENABLE_GAUSSIAN} {
}
proc update_MODELPARAM_VALUE.ENABLE_GAUSSIAN {MODELPARAM_VALUE.ENABLE_GAUSSIAN PARAM_VALUE.ENABLE_GAUSSIAN} {
    set_property value [get_property value ${PARAM_VALUE.ENABLE_GAUSSIAN}] ${MODELPARAM_VALUE.ENABLE_GAUSSIAN}
}

proc validate_PARAM_VALUE.ENABLE_PRBS {PARAM_VALUE.ENABLE_PRBS} {
    if {[get_property value ${PARAM_VALUE.ENABLE_PRBS}] ni {0 1}} { return false }
    return true
}
proc update_PARAM_VALUE.ENABLE_PRBS {PARAM_VALUE.ENABLE_PRBS} {
}
proc update_MODELPARAM_VALUE.ENABLE_PRBS {MODELPARAM_VALUE.ENABLE_PRBS PARAM_VALUE.ENABLE_PRBS} {
    set_property value [get_property value ${PARAM_VALUE.ENABLE_PRBS}] ${MODELPARAM_VALUE.ENABLE_PRBS}
}

proc validate_PARAM_VALUE.ENABLE_BPSK {PARAM_VALUE.ENABLE_BPSK} {
    if {[get_property value ${PARAM_VALUE.ENABLE_BPSK}] ni {0 1}} { return false }
    return true
}
proc update_PARAM_VALUE.ENABLE_BPSK {PARAM_VALUE.ENABLE_BPSK} {
}
proc update_MODELPARAM_VALUE.ENABLE_BPSK {MODELPARAM_VALUE.ENABLE_BPSK PARAM_VALUE.ENABLE_BPSK} {
    set_property value [get_property value ${PARAM_VALUE.ENABLE_BPSK}] ${MODELPARAM_VALUE.ENABLE_BPSK}
}

proc validate_PARAM_VALUE.CHANNELS {PARAM_VALUE.CHANNELS} {
    if {[get_property value ${PARAM_VALUE.CHANNELS}] ni {1 2}} {return false}
    return true
}
proc update_PARAM_VALUE.CHANNELS {PARAM_VALUE.CHANNELS} {}
proc update_MODELPARAM_VALUE.CHANNELS {MODELPARAM_VALUE.CHANNELS PARAM_VALUE.CHANNELS} {
    set_property value [get_property value ${PARAM_VALUE.CHANNELS}] ${MODELPARAM_VALUE.CHANNELS}
}
