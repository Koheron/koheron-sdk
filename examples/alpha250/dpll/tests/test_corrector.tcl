if {$argc != 2} { error "Expected core-directory and output-directory" }
set cores [file normalize [lindex $argv 0]]
set out [file normalize [lindex $argv 1]]
set repo [file normalize [file join [file dirname [info script]] ../../../..]]
create_project -force corrector_test $out -part xc7z020clg400-2
set_property IP_REPO_PATHS $cores [current_project]
update_ip_catalog
create_bd_design corrector
source $repo/fpga/lib/utilities.tcl
source $repo/examples/alpha250/dpll/tcl/corrector.tcl

create_bd_port -dir I -type clk clk
set_property CONFIG.FREQ_HZ 250000000 [get_bd_ports clk]
foreach {name width} {phase_in 32 freq_in 17 sclr 3 p_gain 32 pi_gain 32 i2_gain 32 i3_gain 32} {
    create_bd_port -dir I -from [expr {$width - 1}] -to 0 $name
}
# Keep the original LUT implementation as an independent controller reference.
rename corrector::gain corrector::production_gain
proc corrector::gain {name width low out_width} {
    if {$::legacy_gains} {
        cell xilinx.com:ip:mult_gen:12.0 $name [list \
            PortAWidth $width PortBWidth 32 Multiplier_Construction Use_LUTs \
            OptGoal Speed PipeStages 3 Use_Custom_Output_Width true \
            OutputWidthHigh [expr {$low + $out_width - 1}] OutputWidthLow $low] {}
    } else {
        production_gain $name $width $low $out_width
    }
}
foreach kind {old new} {
    set legacy_gains [expr {$kind eq "old"}]
    corrector::create corr_$kind
    foreach port {clk phase_in freq_in sclr p_gain pi_gain i2_gain i3_gain} {
        connect_bd_net [get_bd_ports $port] [get_bd_pins corr_$kind/$port]
    }
    foreach {port pin width} {
        fast fast_corr 16 slow slow_corr 16
        p proportional/P 32 pi integral/P 32 i2 double_integral/P 32 i3 triple_integral/P 64
        acc1 first_accumulator/Q 48 acc2 second_accumulator/Q 32
    } {
        create_bd_port -dir O -from [expr {$width - 1}] -to 0 ${port}_$kind
        connect_bd_net [get_bd_ports ${port}_$kind] [get_bd_pins corr_$kind/$pin]
    }
}
validate_bd_design
save_bd_design
set bd [get_files */corrector.bd]
generate_target all $bd
add_files [make_wrapper -files $bd -top]
add_files -fileset sim_1 $repo/examples/alpha250/dpll/tests/test_corrector_tb.v
set_property top test_corrector_tb [get_filesets sim_1]
set_property xsim.simulate.runtime all [get_filesets sim_1]
launch_simulation
close_sim
close_project
