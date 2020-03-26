set xpr_filename [lindex $argv 0]
set bit_filename [lindex $argv 1]
set nCPU [lindex $argv 2]

open_project $xpr_filename

if {[get_property PROGRESS [get_runs impl_1]] != "100%"} {
  launch_runs impl_1 -to_step route_design -jobs $nCPU
  wait_on_run impl_1
}

open_run [get_runs impl_1]

set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
set_property BITSTREAM.GENERAL.XADCENHANCEDLINEARITY On [current_design]

write_bitstream -force -file $bit_filename

close_project
