set root [file normalize [file join [file dirname [info script]] ..]]
create_project scale_tests [file normalize tmp/dds-pm-scale-sim] -part xc7z020clg400-2 -force
add_files $root/pm_scale.v
add_files -fileset sim_1 $root/tests/scale_tb.sv
set_property top scale_tb [get_filesets sim_1]
set_property xsim.simulate.runtime 0ns [get_filesets sim_1]
launch_simulation
run all
if {[get_value -radix unsigned /scale_tb/passed] != 1} {error "Scaler test failed"}
close_sim
# Check the mapping that makes the operand capture useful at 250 MHz.
# Moving capture back across the multipliers or losing MREG would restore
# the long selection-to-DSP path or an unpipelined multiplier setup time.
synth_design -top pm_scale -mode out_of_context -part xc7z020clg400-2
set dsps [get_cells -hierarchical -filter {REF_NAME == DSP48E1}]
if {[llength $dsps] != 3} {error "Expected three scaler DSPs, found [llength $dsps]"}
foreach dsp $dsps {
    if {[get_property MREG $dsp] != 1 || [get_property PREG $dsp] != 1} {
        error "Scaler DSP product pipeline not retained: $dsp"
    }
}
set operands [get_cells -hierarchical -filter {NAME =~ g_scale.modulation_sample_reg* && REF_NAME =~ FD*}]
if {[llength $operands] != 25} {error "Waveform capture registers not retained"}
report_utilization -file tmp/dds-pm-scale-sim/utilization.txt
puts {PASS: scaler synthesis retains three DSPs, both product stages and waveform capture}
close_project
