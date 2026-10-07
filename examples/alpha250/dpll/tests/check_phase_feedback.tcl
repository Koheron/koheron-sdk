# Validate only extraction/controller wiring; no full synthesis or implementation.
if {$argc != 2} {error "Expected packaged core directory and output directory"}
set cores [file normalize [lindex $argv 0]]
set out [file normalize [lindex $argv 1]]
set repo [file normalize [file join [file dirname [info script]] ../../../..]]
create_project -force phase_feedback_test $out -part xc7z020clg400-2
set_property IP_REPO_PATHS $cores [current_project]
update_ip_catalog
create_bd_design phase_feedback
source $repo/fpga/lib/utilities.tcl
source $repo/examples/alpha250/dpll/tcl/cordic.tcl
source $repo/examples/alpha250/dpll/tcl/corrector.tcl
create_bd_port -dir I -type clk -freq_hz 250000000 clk
create_bd_port -dir I -type rst resetn
set_property CONFIG.POLARITY ACTIVE_LOW [get_bd_ports resetn]
foreach channel {0 1} {
    cordic::create detector$channel
    connect_cell detector$channel {
        aclk clk
        aresetn resetn
        acc_on [get_constant_pin 1 1]
        s_axis_data_a [get_constant_pin 0 32]
        s_axis_data_b [get_constant_pin 0 32]
        s_axis_tvalid [get_constant_pin 1 1]
        phase_unwrapper/rst [get_constant_pin 0 1]
    }
    corrector::create controller$channel
    connect_cell controller$channel {
        clk clk
        freq_in detector$channel/freq_feedback
        phase_in detector$channel/phase_feedback
        enabled [get_constant_pin 0 3]
        active_banks [get_constant_pin 0 4]
        table_command [get_constant_pin 0 9]
        table_data [get_constant_pin 0 64]
    }
    foreach {pin width} {freq_feedback 25 phase_feedback 40 freq 17 phase 32 m_axis_tdata 32} {
        if {[get_pin_width detector$channel/$pin] != $width} {error "Wrong detector width: $pin"}
    }
    foreach {pin width} {freq_in 25 phase_in 40 fast_corr 16 slow_corr 16} {
        if {[get_pin_width controller$channel/$pin] != $width} {error "Wrong controller width: $pin"}
    }
    foreach {name expected} {PHASE_WIDTH 24 ITERATIONS 24 COMPACT_PREP 0 FUSE_ROUND 1 RESIDUAL_CORRECTION 1} {
        if {[get_property CONFIG.$name [get_bd_cells detector$channel/phase_extractor]] != $expected} {error "Wrong phase configuration: $name"}
    }
    if {[get_property CONFIG.FUSED_DIFFERENCE [get_bd_cells detector$channel/phase_unwrapper]] != 1} {error "Wrong phase difference pipeline"}
    if {[get_property CONFIG.CANONICAL_INPUT [get_bd_cells detector$channel/phase_unwrapper]] != 1} {error "Wrong canonical phase range"}
    if {[get_property CONFIG.TAIL_CARRY_BLOCK [get_bd_cells controller$channel]] != 16} {error "Wrong gain carry blocks"}
    if {[get_property CONFIG.PHASE_FRACTION_BITS [get_bd_cells controller$channel]] != 8} {error "Wrong gain scaling"}
}
validate_bd_design
save_bd_design
puts "Phase feedback wiring checks passed: two 24-bit extractors, 40-bit phase/25-bit frequency controllers, legacy output units preserved"
close_project
