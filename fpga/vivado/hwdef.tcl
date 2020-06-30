set xpr_filename [lindex $argv 0]
set hwdef_filename [lindex $argv 1]
set nCPU [lindex $argv 2]

open_project $xpr_filename

if {[get_property PROGRESS [get_runs synth_1]] != "100%"} {
  launch_runs synth_1 --jobs $nCPU
  wait_on_run synth_1
}

# Workaround to update the STATUS flag without having to reproduce the bitstream
if {[get_property STATUS [get_runs impl_1]] != "write_bitstream Complete!"} {
  launch_runs impl_1 -to_step write_bitstream 
  wait_on_run impl_1
}

write_hw_platform -fixed -include_bit -force -file $hwdef_filename

close_project
