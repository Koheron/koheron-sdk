if {$argc != 2} { error "Expected core-directory and output-directory" }
set cores [file normalize [lindex $argv 0]]
set out [file normalize [lindex $argv 1]]
set repo [file normalize [file join [file dirname [info script]] ../../../..]]
create_project -force cic_test $out -part xc7z020clg400-2
set_property IP_REPO_PATHS $cores [current_project]
update_ip_catalog
create_bd_design cic_test
source $repo/fpga/lib/utilities.tcl
source [file join [file dirname $cores] fpga/memory.tcl]
source $repo/examples/alpha250/dpll/tcl/monitor_filter.tcl
create_bd_port -dir I -type clk clk
set_property CONFIG.FREQ_HZ [get_parameter adc_clk] [get_bd_ports clk]
create_bd_port -dir I -from 31 -to 0 din
create_bd_port -dir I vin
create_bd_port -dir I cfg_valid
foreach rate {4 20 8192} {
    foreach kind {old new} {
        set name cic_${kind}_$rate
        monitor_filter::create_cic $name
        if {$kind eq "old"} {
            set_property CONFIG.Use_Xtreme_DSP_Slice false [get_bd_cells $name]
        }
        connect_cell $name {
            aclk clk s_axis_data_tdata din s_axis_data_tvalid vin
            s_axis_config_tvalid cfg_valid
        }
        connect_bd_net [get_bd_pins [get_constant_pin $rate 16]] [get_bd_pins $name/s_axis_config_tdata]
        connect_bd_net [get_bd_pins [get_constant_pin 1 1]] [get_bd_pins $name/m_axis_data_tready]
        foreach {port pin width} {
            ready s_axis_data_tready 1 cfg_ready s_axis_config_tready 1
            dout m_axis_data_tdata 32 valid m_axis_data_tvalid 1
        } {
            create_bd_port -dir O -from [expr {$width - 1}] -to 0 ${port}_${kind}_$rate
            connect_bd_net [get_bd_ports ${port}_${kind}_$rate] [get_bd_pins $name/$pin]
        }
    }
}
validate_bd_design
save_bd_design
set bd [get_files */cic_test.bd]
generate_target all $bd
add_files [make_wrapper -files $bd -top]
add_files -fileset sim_1 $repo/examples/alpha250/dpll/tests/test_cic_tb.v
set_property top test_cic_tb [get_filesets sim_1]
set_property xsim.simulate.runtime all [get_filesets sim_1]
launch_simulation
close_sim
close_project
