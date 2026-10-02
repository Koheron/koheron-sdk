# Supply a repository containing the packaged awg IP as the first argument.
set root [file normalize [file join [file dirname [info script]] ..]]
set profiles {1 1 2 1 2 0}
set suffix {}
if {[lindex $argv 1] eq "profile"} {set profiles {2 1}; set suffix -profile}
foreach {channels sine} $profiles {
    create_project subsystem_tests [file normalize tmp/dds-pm-subsystem-$channels-$sine$suffix] -part xc7z020clg400-2 -force
    set_property ip_repo_paths [file normalize [lindex $argv 0]] [current_project]
    update_ip_catalog
    create_ip -vlnv koheron:user:awg:1.0 -module_name pm_subsystem
    set_property -dict [list CONFIG.CHANNELS $channels CONFIG.ENABLE_SINE $sine] [get_ips pm_subsystem]
    generate_target simulation [get_ips pm_subsystem]
    add_files -fileset sim_1 $root/tests/subsystem_tb.sv
    set phase_width [get_property CONFIG.PHASE_WIDTH [get_ips pm_subsystem]]
    set output_width [get_property CONFIG.OUTPUT_WIDTH [get_ips pm_subsystem]]
    set defines [list CHANNEL_TEST_COUNT=$channels SINE_TEST_ENABLE=$sine \
        PROFILE_PHASE_WIDTH=$phase_width PROFILE_OUTPUT_WIDTH=$output_width]
    if {$channels == 2} {lappend defines TWO_CHANNELS}
    set_property verilog_define $defines [get_filesets sim_1]
    set_property top subsystem_tb [get_filesets sim_1]
    set_property xsim.simulate.runtime 0ns [get_filesets sim_1]
    launch_simulation
    run all
    if {[get_value -radix unsigned /subsystem_tb/passed] != 1} {error "Subsystem test failed: channels=$channels sine=$sine"}
    close_sim
    # Synthesize the same catalog instance, including its nested vendor IPs.
    # This catches unresolved black boxes and verifies single-channel pruning.
    generate_target synthesis [get_ips pm_subsystem]
    synth_design -top pm_subsystem -mode out_of_context -part xc7z020clg400-2
    set blackboxes [get_cells -quiet -hier -filter {IS_BLACKBOX}]
    if {[llength $blackboxes]} {error "Unresolved subcores: $blackboxes"}
    set controllers [get_cells -hier -filter {ORIG_REF_NAME == awg_control}]
    if {[llength $controllers] != $channels} {error "Channel count did not prune hardware"}
    report_utilization -file tmp/dds-pm-subsystem-$channels-$sine$suffix/utilization.txt
    close_project
}
puts {PASS: packaged subsystem simulation and synthesis profiles}
