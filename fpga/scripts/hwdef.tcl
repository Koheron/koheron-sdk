
set instrument_name [lindex $argv 0]

open_instrument tmp/$instrument_name.xpr

if {[get_property PROGRESS [get_runs synth_1]] != "100%"} {
  launch_runs synth_1
  wait_on_run synth_1
}

write_hwdef -force -file tmp/$instrument_name.hwdef

close_instrument
