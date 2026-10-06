foreach source {table_gain.v table_corrector.v gain_programmer.v} {
    add_files -norecurse [file normalize [file join [file dirname [info script]] .. $source]]
}

namespace eval corrector {
proc create {module_name} {
    create_bd_cell -type module -reference table_corrector $module_name
    set_cell_props $module_name {
        FUSED 1
        GAIN_STAGES 2
        TAIL_GAIN_STAGES 3
        FINAL_CSA_LEVELS 2
        CARRY_BLOCK 0
    }
}
}
