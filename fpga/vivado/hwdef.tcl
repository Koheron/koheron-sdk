set xpr_filename [lindex $argv 0]
set hwdef_filename [lindex $argv 1]
set nCPU [lindex $argv 2]

open_project $xpr_filename

if {[get_property PROGRESS [get_runs synth_1]] != "100%"} {
<<<<<<< HEAD
  launch_runs synth_1 --jobs $nCPU
=======
  launch_runs synth_1 -jobs $nCPU
>>>>>>> d8bf2165890c46bf522f1053ba7f3597f99ddbb7
  wait_on_run synth_1
}

write_hw_platform -fixed -force -file $hwdef_filename

close_project
