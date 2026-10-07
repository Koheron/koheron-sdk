foreach source {table_gain.v table_corrector.v gain_programmer.v fast_p_detector.v p_path_switch.v manual_p_corrector.v} {
    add_files -norecurse [file normalize [file join [file dirname [info script]] .. $source]]
}

namespace eval corrector {
proc create {module_name} {
    create_bd_cell -type module -reference manual_p_corrector $module_name
    set_cell_props $module_name {
        FUSED 1
        GAIN_STAGES 4
        FAST_GAIN_STAGES 3
        TAIL_GAIN_STAGES 4
        I2_GAIN_STAGES 4
        FINAL_CSA_LEVELS 2
        CARRY_BLOCK 0
        FAST_P_DSP 1
        PIPELINED_REFERENCE 1
        PRECOMBINE_I 1
        SELECTOR_CARRY_BLOCK 0
        FREQ_WIDTH 25
        PHASE_WIDTH 40
        PHASE_FRAC 8
    }
}
}
