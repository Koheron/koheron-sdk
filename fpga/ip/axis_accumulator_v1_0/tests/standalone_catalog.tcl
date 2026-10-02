# Run against an extracted export ZIP, not the SDK's generated core directory.
# This consumer uses only standard Vivado commands and two native AXIS instances.
if {[llength $argv] != 2} {error {Expected: <extracted IP repository> <test output directory>}}
set repo [file normalize [lindex $argv 0]]
set out [file normalize [lindex $argv 1]]
create_project -force standalone_catalog $out -part xc7z010clg400-1
set_property ip_repo_paths $repo [current_project]
update_ip_catalog
create_bd_design native
set clock [create_bd_port -dir I -type clk aclk]
set reset [create_bd_port -dir I -type rst aresetn]
set_property CONFIG.FREQ_HZ 125000000 $clock
set_property CONFIG.ASSOCIATED_RESET aresetn $clock
set_property CONFIG.POLARITY ACTIVE_LOW $reset
set buses {}
foreach {name bins frames sync} {accum_0 16 3 0 accum_1 64 7 1} {
    set ip [create_bd_cell -type ip -vlnv koheron:user:axis_accumulator:1.0 $name]
    set_property -dict [list CONFIG.FRAME_LENGTH $bins CONFIG.N_FRAMES $frames \
        CONFIG.CHECK_TLAST 1 CONFIG.SYNC_ON_RESET $sync] $ip
    connect_bd_net $clock [get_bd_pins $name/aclk]
    connect_bd_net $reset [get_bd_pins $name/aresetn]
    foreach {interface mode} {S_AXIS Slave M_AXIS Master} {
        set port [create_bd_intf_port -mode $mode -vlnv xilinx.com:interface:axis_rtl:1.0 ${name}_$interface]
        connect_bd_intf_net $port [get_bd_intf_pins $name/$interface]
        lappend buses ${name}_$interface
    }
    if {[get_property CONFIG.FRAME_LENGTH $ip] != $bins ||
        [get_property CONFIG.N_FRAMES $ip] != $frames} {error {Catalog customization failed}}
}
set_property CONFIG.ASSOCIATED_BUSIF [join $buses :] $clock
validate_bd_design
save_bd_design
set_property synth_checkpoint_mode None [get_files native.bd]
generate_target all [get_files native.bd]
add_files [make_wrapper -files [get_files native.bd] -top]
set_property top native_wrapper [current_fileset]
update_compile_order -fileset sources_1
synth_design -top native_wrapper -mode out_of_context -part xc7z010clg400-1
if {[llength [get_cells -quiet -hier -filter {IS_BLACKBOX == 1}]]} {
    error {Exported IP has unresolved black boxes}
}
report_utilization -file $out/utilization.rpt
write_checkpoint -force $out/native.dcp
puts {PASS: relocated archive, native AXIS block design, independent instances and different target part}
close_project
