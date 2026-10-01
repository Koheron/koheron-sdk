# Check the real DDS IP against the sine reference, without a board.
set example [file normalize [file join [file dirname [info script]] ..]]
create_project dds_tests [file normalize tmp/adc-dma-chirp-dds-sim] -part xc7z020clg400-2 -force
create_ip -name dds_compiler -vendor xilinx.com -library ip -version 6.0 -module_name test_sine_lut
# Keep identical to sine_lut in block_design.tcl.
set_property -dict [list CONFIG.PartsPresent SIN_COS_LUT_only \
    CONFIG.Output_Selection Sine CONFIG.Parameter_Entry Hardware_Parameters \
    CONFIG.Phase_Width 16 CONFIG.Output_Width 16 CONFIG.Noise_Shaping None \
    CONFIG.Has_Phase_Out false CONFIG.Has_ARESETn true \
    CONFIG.S_PHASE_Has_TUSER User_Field CONFIG.S_PHASE_TUSER_Width 1 \
    CONFIG.M_DATA_Has_TUSER User_Field \
    CONFIG.Latency_Configuration Configurable CONFIG.Latency 8] [get_ips test_sine_lut]
generate_target simulation [get_ips test_sine_lut]
add_files -fileset sim_1 $example/tests/dds_lut_tb.sv
set_property top dds_lut_tb [get_filesets sim_1]
set_property xsim.simulate.runtime 0ns [get_filesets sim_1]
launch_simulation
run all
if {[get_value -radix unsigned /dds_lut_tb/passed] != 1} { error "DDS test failed" }
close_sim
close_project
