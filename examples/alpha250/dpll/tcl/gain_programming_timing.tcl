# gain_programmer prepares fields before its registered strobe. The RAM captures
# them two clocks after preparation. Strobes, bank commits and feedback retain
# the normal 4 ns requirement. Apply before placement and again after replication.
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
    if {[llength $held_sources] < 80 || ![llength $write_pins]} {error "Missing held gain programming inputs"}
    set_multicycle_path 2 -setup -from $held_sources -to $write_pins
    set_multicycle_path 1 -hold -from $held_sources -to $write_pins
    puts "DPLL held programming timing: [llength $held_sources] sources, [llength $write_pins] physical RAM pins"
}
dpll_constrain_gain_programming
