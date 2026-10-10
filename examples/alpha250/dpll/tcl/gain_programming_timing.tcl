# gain_programmer prepares fields before its registered strobe. The RAM captures
# them three clocks after preparation (or the retimed table write port does).
# Strobes, bank commits and feedback retain the normal 4 ns requirement.
# Apply before placement and again after replication.
proc dpll_constrain_gain_programming {} {
    set programming_sources [get_cells -hier -filter {REF_NAME == FDRE && (NAME =~ *gain_programmer/inst/data_reg* || NAME =~ *gain_programmer/inst/command*_reg*)}]
    set held_sources {}
    foreach source $programming_sources {
        if {[regexp {/(data_reg\[|command[01]_reg\[[0-7]\])} $source]} {lappend held_sources $source}
    }
    # Select physical RAM primitives, excluding their enclosing RAM32M macros.
    # The macro WE pins have no timing checks and are invalid exception endpoints.
    set gain_memories [get_cells -hier -filter {IS_PRIMITIVE && (REF_NAME =~ RAMD* || REF_NAME =~ RAMS*) && (NAME =~ *tables.chunk*.products_reg* || NAME =~ *dsp_p.coefficients_reg*)}]
    set write_pins [get_pins -leaf -of_objects $gain_memories -filter {REF_PIN_NAME =~ WADR* || REF_PIN_NAME =~ ADR* || REF_PIN_NAME == I || REF_PIN_NAME == WE}]
    set write_registers [get_cells -hier -filter {REF_NAME == FDRE && NAME =~ *tables.write_stage.* && (NAME =~ *data_q_reg* || NAME =~ *address_q_reg* || NAME =~ *bank_q_reg* || NAME =~ *signed_q_reg*)}]
    set write_pins [concat $write_pins [get_pins -of_objects $write_registers -filter {REF_PIN_NAME == D}]]
    # Tables consume 48 payload bits. Coefficient readback now comes directly
    # from the coherent response, so unused upper payload registers disappear.
    if {[llength $held_sources] < 64 || ![llength $write_pins]} {error "Missing held gain programming inputs"}
    set_multicycle_path 3 -setup -from $held_sources -to $write_pins
    set_multicycle_path 2 -hold -from $held_sources -to $write_pins
    puts "DPLL held programming timing: [llength $held_sources] sources, [llength $write_pins] RAM/write-stage pins"
}
dpll_constrain_gain_programming
