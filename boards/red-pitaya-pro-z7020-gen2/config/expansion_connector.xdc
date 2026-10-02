source [file normalize [file join [file dirname [info script]] .. .. red-pitaya config expansion_connector.xdc]]

# Additional E1 GPIOs use Bank 13, populated at 2.5 V on Gen2 Z7020.
# Constrain only ports actually exposed by the instrument.
foreach {side pins} {p {Y9 Y12 Y7} n {Y8 Y13 Y6}} {
  for {set i 0} {$i < 3} {incr i} {
    set port [get_ports -quiet [format {exp_%s_tri_io[%d]} $side [expr {$i + 8}]]]
    if {[llength $port]} {
      set_property -dict [list PACKAGE_PIN [lindex $pins $i] IOSTANDARD LVCMOS25 SLEW FAST DRIVE 8] $port
    }
  }
}
