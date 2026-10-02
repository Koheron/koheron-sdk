# Supply a packaged SDK core directory as the first argument.
set root [file normalize [file join [file dirname [info script]] ../../../..]]
create_project smoke_tests [file normalize tmp/dds-pm-smoke] -part xc7z020clg400-2 -force
set_property ip_repo_paths [file normalize [lindex $argv 0]] [current_project]
update_ip_catalog
source $root/fpga/lib/utilities.tcl
source $root/fpga/ip/awg_v1_0/integration.tcl
create_bd_design smoke
cell xilinx.com:ip:processing_system7:5.5 ps {PCW_USE_M_AXI_GP0 1} {}
set ps_clk0 ps/FCLK_CLK0
set rst0_name reset
cell xilinx.com:ip:proc_sys_reset:5.0 reset {} {slowest_sync_clk ps/FCLK_CLK0}
cell xilinx.com:ip:axi_interconnect:2.1 axi_mem_intercon_0 {NUM_MI 1} {
    S00_AXI ps/M_AXI_GP0 ACLK ps/FCLK_CLK0 S00_ACLK ps/FCLK_CLK0
    ARESETN reset/peripheral_aresetn S00_ARESETN reset/peripheral_aresetn
}
connect_pins ps/M_AXI_GP0_ACLK ps/FCLK_CLK0
connect_cell reset [list ext_reset_in [get_constant_pin 0 1]]
namespace eval config {
    set memory_test_offset 0x60000000
    set memory_test_range 8K
}
set outputs [dds_pm::add combined test ps/FCLK_CLK0 125000000 {
    CHANNELS 2 ENABLE_SINE 0 ENABLE_GAUSSIAN 0
}]
if {[llength $outputs] != 2} {error {Wrong DAC output count}}
if {[llength [get_bd_cells -quiet combined_carrier]]} {error {Carrier escaped the parent IP}}
if {[llength [get_bd_cells -quiet combined_modulation]]} {error {Modulation LUT escaped the parent IP}}
if {[get_property CONFIG.CHANNELS [get_bd_cells combined]] != 2} {error {Channel count mismatch}}
if {[llength [get_bd_intf_pins combined/*]] != 1} {error {Internal stream interfaces exposed}}
validate_bd_design
puts {PASS: integrated packaged DDS PM block design}
