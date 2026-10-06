if {$argc != 2} { error "Expected core-directory and output-directory" }
set cores [file normalize [lindex $argv 0]]
set out [file normalize [lindex $argv 1]]
set repo [file normalize [file join [file dirname [info script]] ../../../..]]
create_project -force monitor_test $out -part xc7z020clg400-2
set_property IP_REPO_PATHS $cores [current_project]
update_ip_catalog
create_bd_design monitor
source $repo/fpga/lib/utilities.tcl
source [file join [file dirname $cores] fpga/memory.tcl]
source $repo/examples/alpha250/dpll/tcl/monitor_filter.tcl

set python $::env(DPLL_TEST_PYTHON)
set coeffs [exec -- env -i $python -I $repo/fpga/scripts/fir.py \
    [get_parameter cic_n_stages] [get_parameter cic_decimation_rate_default] \
    [get_parameter cic_differential_delay] print]
create_bd_port -dir I -type clk clk
set_property CONFIG.FREQ_HZ [get_parameter adc_clk] [get_bd_ports clk]
foreach kind {old new} {
    monitor_filter::create_fir fir_$kind $coeffs
    if {$kind eq "old"} {
        # Original monitoring architecture, fed slowly enough to avoid stalls.
        set_property -dict {CONFIG.Clock_Frequency 143 CONFIG.Sample_Frequency 10} [get_bd_cells fir_old]
    }
    connect_bd_net [get_bd_ports clk] [get_bd_pins fir_$kind/aclk]
    foreach {port pin dir width} {
        din s_axis_data_tdata I 32 vin s_axis_data_tvalid I 1
        rin s_axis_data_tready O 1 dout m_axis_data_tdata O 32 vout m_axis_data_tvalid O 1
    } {
        create_bd_port -dir $dir -from [expr {$width - 1}] -to 0 ${port}_$kind
        connect_bd_net [get_bd_ports ${port}_$kind] [get_bd_pins fir_$kind/$pin]
    }
    connect_bd_net [get_bd_pins [get_constant_pin 1 1]] [get_bd_pins fir_$kind/m_axis_data_tready]
}
validate_bd_design
save_bd_design
set bd [get_files */monitor.bd]
generate_target all $bd
add_files [make_wrapper -files $bd -top]
add_files -fileset sim_1 $repo/examples/alpha250/dpll/tests/test_monitor_tb.v
set_property top test_monitor_tb [get_filesets sim_1]
set_property xsim.simulate.runtime all [get_filesets sim_1]
launch_simulation
close_sim
close_project
