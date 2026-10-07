if {$argc != 2} { error "Expected core-directory and output-directory" }
set cores [file normalize [lindex $argv 0]]
set out [file normalize [lindex $argv 1]]
set repo [file normalize [file join [file dirname [info script]] ../../../..]]

create_project -force detector_test $out -part xc7z020clg400-2
set_property IP_REPO_PATHS $cores [current_project]
update_ip_catalog
create_bd_design detector
source $repo/fpga/lib/utilities.tcl
source $repo/examples/alpha250/dpll/tcl/cordic.tcl

create_bd_port -dir I -type clk aclk
set_property CONFIG.FREQ_HZ 250000000 [get_bd_ports aclk]
create_bd_port -dir I -type rst aresetn
set_property CONFIG.POLARITY ACTIVE_LOW [get_bd_ports aresetn]
foreach name {acc_on valid} { create_bd_port -dir I $name }
foreach name {data_a data_b} { create_bd_port -dir I -from 31 -to 0 $name }

foreach kind {old new fast} {
    # Keep both historical 16-bit vendor detectors as regression references.
    cordic::create det_$kind [expr {$kind eq "fast" ? "fast" : "vendor"}] [expr {$kind eq "fast" ? 24 : 16}] [expr {$kind eq "fast" ? 24 : 16}]
    if {$kind eq "old"} {
        foreach i {0 1} { set_property CONFIG.LOW_LATENCY 0 [get_bd_cells det_old/boxcar$i] }
        set_property CONFIG.Pipelining_Mode Maximum [get_bd_cells det_old/cordic]
    } elseif {$kind eq "new"} {
        foreach i {0 1} {
            if {[get_property CONFIG.LOW_LATENCY [get_bd_cells det_new/boxcar$i]] != 1} {
                error "Production detector must use the low-latency boxcar"
            }
        }
        if {[get_property CONFIG.Pipelining_Mode [get_bd_cells det_new/cordic]] ne "Optimal"} {
            error "Production detector must use optimal CORDIC pipelining"
        }
    } else {
        foreach {name property expected} {
            complex_mult OutputWidth 24
            boxcar0 DATA_WIDTH 24
            boxcar1 DATA_WIDTH 24
            phase_extractor INPUT_WIDTH 24
            phase_extractor PHASE_WIDTH 24
            phase_extractor ITERATIONS 24
            phase_extractor ROTATIONS_PER_CLOCK 2
            phase_extractor PAIR_START 8
            phase_extractor FUSE_ROUND 1
            phase_extractor COMPACT_PREP 0
            phase_extractor RESIDUAL_CORRECTION 1
            phase_unwrapper FUSED_DIFFERENCE 1
        phase_unwrapper CANONICAL_INPUT 1
        } {
            if {[get_property CONFIG.$property [get_bd_cells det_fast/$name]] != $expected} {
                error "Wrong 24-bit detector configuration: $name $property"
            }
        }
    }
    foreach {port pin} {aclk aclk aresetn aresetn acc_on acc_on valid s_axis_tvalid data_a s_axis_data_a data_b s_axis_data_b} {
        connect_bd_net [get_bd_ports $port] [get_bd_pins det_$kind/$pin]
    }
    # Match the current production phase-unwrapper reset connection.
    connect_bd_net [get_bd_pins [get_constant_pin 0 1]] [get_bd_pins det_$kind/phase_unwrapper/rst]
    foreach {port pin width} {phase phase 32 freq freq 17 valid m_axis_tvalid 1} {
        create_bd_port -dir O -from [expr {$width - 1}] -to 0 ${port}_$kind
        connect_bd_net [get_bd_ports ${port}_$kind] [get_bd_pins det_$kind/$pin]
    }
}

foreach {port pin width} {phase_precise phase_feedback 40 freq_precise freq_feedback 25} {
    create_bd_port -dir O -from [expr {$width-1}] -to 0 $port
    connect_bd_net [get_bd_ports $port] [get_bd_pins det_fast/$pin]
}

# Observe the filtered Cartesian values to check that fractional mixer bits
# reach extraction through both 24-bit lanes, rather than padding 16-bit IQ.
foreach {port boxcar} {iq_i_fast boxcar0 iq_q_fast boxcar1} {
    create_bd_port -dir O -from 23 -to 0 $port
    connect_bd_net [get_bd_ports $port] [get_bd_pins det_fast/$boxcar/dout]
}

validate_bd_design
save_bd_design
set bd [get_files */detector.bd]
generate_target all $bd
add_files [make_wrapper -files $bd -top]
add_files -fileset sim_1 $repo/examples/alpha250/dpll/tests/test_detector_tb.v
set_property top test_detector_tb [get_filesets sim_1]
set_property xsim.simulate.runtime all [get_filesets sim_1]
launch_simulation
close_sim
close_project
