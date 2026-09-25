set xpr_filename [lindex $argv 0]
set bit_filename [lindex $argv 1]
set type [lindex $argv 2]
set nCPU [lindex $argv 3]


open_project $xpr_filename

set impl_run [get_runs impl_1]
if {[get_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.IS_ENABLED $impl_run]} {
  set final_step {phys_opt_design (Post-Route)}
} else {
  set final_step route_design
}

if {[get_property PROGRESS $impl_run] != "100%"} {
  launch_runs $impl_run -to_step $final_step -jobs $nCPU
  wait_on_run impl_1
}

open_run $impl_run

source [file join [file dirname [info script]] timing_check.tcl]
koheron_check_routed_timing $impl_run $bit_filename

set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
if {$type == "zynq"} {
  set_property BITSTREAM.GENERAL.XADCENHANCEDLINEARITY On [current_design]
}
write_bitstream -force -file $bit_filename

close_project
