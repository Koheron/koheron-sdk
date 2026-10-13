# Optional E3 differential interfaces; names and ordering follow the official
# Gen2 FPGA top. Instantiate differential I/O buffers in the instrument.
foreach {name pins} {
  exp_e3p_o {T5 U9 V8 U7}
  exp_e3n_o {U5 U8 W8 V7}
  exp_e3p_i {V11 W11 W10 T9}
  exp_e3n_i {V10 Y11 W9 U10}
} {
  for {set i 0} {$i < 4} {incr i} {
    set port [get_ports -quiet [format {%s[%d]} $name $i]]
    if {[llength $port]} {
      set_property -dict [list PACKAGE_PIN [lindex $pins $i] IOSTANDARD LVDS_25] $port
    }
  }
}
foreach {name pin} {s1_orient_i W6 s1_link_i V6} {
  set port [get_ports -quiet $name]
  if {[llength $port]} {
    set_property -dict [list PACKAGE_PIN $pin IOSTANDARD LVCMOS25] $port
  }
}
