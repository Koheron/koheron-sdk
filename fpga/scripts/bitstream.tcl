
set instrument_name [lindex $argv 0]

open_instrument tmp/$instrument_name.xpr

if {[get_property PROGRESS [get_runs impl_1]] != "100%"} {
  launch_runs impl_1 -to_step route_design
  wait_on_run impl_1
}

open_run [get_runs impl_1]

set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
set_property BITSTREAM.GENERAL.XADCENHANCEDLINEARITY On [current_design]

write_bitstream -force -file tmp/$instrument_name.bit

close_instrument
