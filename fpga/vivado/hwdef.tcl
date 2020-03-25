set xpr_filename [lindex $argv 0]
set hwdef_filename [lindex $argv 1]
set nCPU [lindex $argv 2]

open_project $xpr_filename

if {[get_property PROGRESS [get_runs synth_1]] != "100%"} {
  launch_runs synth_1 -jobs $nCPU
  wait_on_run synth_1
}

write_hwdef -force -file $hwdef_filename

close_project
