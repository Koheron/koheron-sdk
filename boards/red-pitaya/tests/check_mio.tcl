# Validate the actual Red Pitaya PS preset without requiring Vivado.
# MIO 0..15 use the 3.3 V bank; MIO 16..53 use the 2.5 V bank.
set ps_name ps7
set properties {}
proc get_bd_cells {name} { return $name }
proc set_property {option values cell} {
    if {$option ne "-dict"} { error "Expected a property dictionary" }
    set ::properties $values
}
set preset [file normalize [file join [file dirname [info script]] ../config/board_preset.tcl]]
if {$argc == 1} {
    set preset [lindex $argv 0]
} elseif {$argc != 0} {
    error "Usage: tclsh check_mio.tcl ?preset.tcl?"
}
source $preset
if {[dict get $properties CONFIG.PCW_PRESET_BANK1_VOLTAGE] ne "LVCMOS 2.5V"} {
    error "Red Pitaya MIO bank 1 must use LVCMOS 2.5V"
}
for {set pin 0} {$pin < 54} {incr pin} {
    set expected [expr {$pin < 16 ? "LVCMOS 3.3V" : "LVCMOS 2.5V"}]
    set actual [dict get $properties CONFIG.PCW_MIO_${pin}_IOTYPE]
    if {$actual ne $expected} {
        error "MIO${pin} IOTYPE: expected '$expected', got '$actual'"
    }
    set direction [dict get $properties CONFIG.PCW_MIO_${pin}_DIRECTION]
    if {$direction ni {in out inout}} {
        error "MIO${pin} DIRECTION: invalid value '$direction'"
    }
}
puts "Red Pitaya: all 54 MIO electrical types and directions are valid"
