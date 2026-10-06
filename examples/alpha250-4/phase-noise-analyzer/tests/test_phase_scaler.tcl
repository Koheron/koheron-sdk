if {$argc != 1} {error "Expected the full PNA project path"}
set root [file normalize [file join [file dirname [info script]] ../../../..]]
set project [file normalize [lindex $argv 0]]
set input [file rootname $project].srcs/sources_1/bd/system/ip/system_scaler0_0/system_scaler0_0.xci
if {![file exists $input]} {error "Missing production scaler IP; run make xpr first"}
set out $root/tmp/tests/alpha250-4-phase-noise-analyzer/scaler
create_project -force phase_scaler_test $out -part xc7z020clg400-2
cd $out
import_ip -files $input
generate_target simulation [get_ips]
add_files -fileset sim_1 $root/examples/alpha250-4/phase-noise-analyzer/tests/test_phase_scaler_tb.sv
set_property top test_phase_scaler_tb [get_filesets sim_1]
set_property xsim.simulate.runtime all [get_filesets sim_1]
launch_simulation
close_sim
source $root/examples/alpha250/dpll/tests/check_simulation.tcl
check_simulation $out/phase_scaler_test.sim/sim_1/behav/xsim/simulate.log \
    {PASS: production DSP phase scaler preserves signed 64x32 arithmetic and eight-clock delay (4096 samples)}
close_project
