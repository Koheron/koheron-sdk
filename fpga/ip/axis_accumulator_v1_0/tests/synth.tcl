set repo [file normalize [lindex $argv 0]]
set out [file normalize [lindex $argv 1]]
if {![file exists $repo/axis_accumulator_v1_0/component.xml]} {error {Generate the package first}}
foreach bins {2048 8192} {
    create_project -force accumulator_synth $out/$bins -part xc7z020clg400-2
    set_property ip_repo_paths $repo [current_project]
    update_ip_catalog
    create_ip -vlnv koheron:user:axis_accumulator:1.0 -module_name accumulator_dut
    set_property -dict [list CONFIG.FRAME_LENGTH $bins CONFIG.N_FRAMES 1023] [get_ips accumulator_dut]
    generate_target synthesis [get_ips accumulator_dut]
    synth_design -top accumulator_dut -mode out_of_context -part xc7z020clg400-2
    if {[llength [get_cells -quiet -hier -filter {IS_BLACKBOX == 1}]]} {error {Unresolved vendor IP black boxes}}
    create_clock -name stream_clk -period 4.0 [get_ports aclk]
    opt_design
    place_design
    phys_opt_design
    route_design
    report_utilization -file $out/$bins/utilization.rpt
    report_timing_summary -file $out/$bins/timing.rpt
    set setup [get_property SLACK [get_timing_paths -delay_type max -max_paths 1]]
    set hold [get_property SLACK [get_timing_paths -delay_type min -max_paths 1]]
    puts "Accumulator $bins bins at 250 MHz: setup=$setup ns hold=$hold ns"
    if {$setup < 0 || $hold < 0} {error {Accumulator failed constrained internal timing}}
    write_checkpoint -force $out/$bins/accumulator.dcp
    close_project
}
