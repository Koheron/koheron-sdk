source [file join [file dirname [info script]] .. timing_check.tcl]

# Model the optimized two-bit response bus: one source flop, one live
# destination flop, and one destination tied low. No Vivado license required.
proc get_cells {args} {
  if {[lindex $args 0] eq "-of_objects"} {
    return [lindex [split [lindex $args 1] /] 0]
  }
  return [lindex $args end]
}
proc get_pins {args} {
  set object [lindex $args [expr {[lsearch $args -of_objects] + 1}]]
  if {[lindex $args end] eq "REF_PIN_NAME == D"} { return "$object/D" }
  return $::drivers($object)
}
proc get_nets {args} { return [lindex $args end] }
proc get_property {property object} {
  if {$property eq "REF_NAME"} {
    if {$object eq "gnd"} { return GND }
    return FDRE
  }
  return $::slack
}
proc get_timing_paths {args} { return $::paths }
proc check {expected description} {
  set actual [koheron_single_bit_skew $::entry]
  if {$actual != $expected} { error "$description: expected $expected, got $actual" }
  puts "PASS: $description"
}
set entry {6 30 [get_cells {src}]
  [get_cells [list {dst0} {dst1}]]
  NA 15.999 NA NA}
set drivers(dst0/D) gnd/G
set drivers(dst1/D) src/Q
set slack 14.597
set paths path
check 1 {one timed bit and one constant bit}
set drivers(dst0/D) src/Q
check 0 {multiple live destinations require bus skew analysis}
set drivers(dst0/D) gnd/G
set drivers(dst1/D) other/Q
check 0 {unrelated source rejected}
set drivers(dst1/D) src/Q
set slack -0.1
check 0 {negative slack rejected}
set slack NA
check 0 {unavailable slack rejected}
set slack 14.597
set paths {}
check 0 {unconstrained path rejected}
set entry {6 30 [get_cells [list {src0} {src1}]]}
check 0 {multiple source bits rejected}
