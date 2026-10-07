if {$argc != 2} {error "Expected full-instrument project and report directory"}
set project [file normalize [lindex $argv 0]]
set out [file normalize [lindex $argv 1]]
file mkdir $out
open_project $project
if {[get_property top [current_fileset]] ne "system_wrapper"} {
    error "The instrument wrapper must be the synthesis top"
}
open_bd_design [get_files */system.bd]
foreach channel {0 1} {
    foreach {name expected} {INPUT_WIDTH 24 PHASE_WIDTH 24 ITERATIONS 24 ROTATIONS_PER_CLOCK 2 PAIR_START 8 FUSE_ROUND 1 COMPACT_PREP 0 RESIDUAL_CORRECTION 1} {
        if {[get_property CONFIG.$name [get_bd_cells cordic$channel/phase_extractor]] != $expected} {
            error "Incorrect phase extractor parameter: loop $channel $name"
        }
    }
    foreach {cell property} {complex_mult OutputWidth boxcar0 DATA_WIDTH boxcar1 DATA_WIDTH} {
        if {[get_property CONFIG.$property [get_bd_cells cordic$channel/$cell]] != 24} {
            error "Incorrect Cartesian width in loop $channel/$cell"
        }
    }
    foreach {name expected} {DIN_WIDTH 24 DOUT_WIDTH 40 FUSED_DIFFERENCE 1 CANONICAL_INPUT 1} {
        if {[get_property CONFIG.$name [get_bd_cells cordic$channel/phase_unwrapper]] != $expected} {
            error "Incorrect phase scale in loop $channel"
        }
    }
    foreach {name expected} {PHASE_FRACTION_BITS 8 FUSED 1 GAIN_STAGES 2 TAIL_GAIN_STAGES 3 FINAL_CSA_LEVELS 2 CARRY_BLOCK 0 TAIL_CARRY_BLOCK 8} {
        if {[get_property CONFIG.$name [get_bd_cells corrector$channel]] != $expected} {
            error "Incorrect controller parameter: loop $channel $name"
        }
    }
}
open_run impl_1
set adc_clocks [get_clocks clk_out1_system_mmcm_0*]
if {[llength $adc_clocks] == 0} {error "Missing ADC clock constraints"}
foreach clock $adc_clocks {
    if {[get_property PERIOD $clock] != 4.0} {error "Expected 250 MHz ADC clocks"}
}
report_utilization -hierarchical -file $out/utilization.rpt
report_timing_summary -delay_type min_max -report_unconstrained -file $out/timing.rpt
set result [open $out/result.txt w]
puts $result "top=[get_property top [current_fileset]] part=[get_property PART [current_project]]"
foreach kind {max min} {
    set path [get_timing_paths -delay_type $kind -max_paths 1 -no_report_unconstrained]
    if {[llength $path] != 1 || [get_property SLACK $path] < 0} {
        error "Full-instrument $kind timing failed"
    }
    puts $result "$kind slack=[get_property SLACK $path] ns"
}
foreach channel {0 1} {
    set phase_prefix "system_i/cordic$channel/phase_extractor/inst"
    set phase_cells [get_cells -hier -filter "NAME =~ $phase_prefix/* && IS_PRIMITIVE"]
    if {[llength $phase_cells] == 0} {error "Missing custom phase extractor in loop $channel"}
    if {[llength [filter $phase_cells {REF_NAME == DSP48E1}]] != 2} {
        error "Expected two residual-correction DSPs in phase extractor loop $channel"
    }
    # Keep the interpolation input registers in the DSP. Extracting these
    # into fabric leaves a multiply-plus-add input path at 250 MHz.
    foreach {mac registers} {
        interpolation {AREG 1 BREG 1 CREG 1 MREG 0 PREG 1}
        final_angle {AREG 1 BREG 0 CREG 1 MREG 1 PREG 1}
    } {
        set dsp [get_cells "$phase_prefix/residual_completion.completion/$mac/dsp"]
        if {[llength $dsp] != 1} {error "Missing residual DSP: loop $channel $mac"}
        foreach {name expected} $registers {
            if {[get_property $name $dsp] != $expected} {
                error "Residual DSP register changed: loop $channel $mac $name"
            }
        }
    }
    set phase_pins [get_pins -hier -filter "NAME =~ $phase_prefix/*"]
    set phase_path [get_timing_paths -through $phase_pins -max_paths 1 -no_report_unconstrained]
    if {[llength $phase_path] != 1 || [get_property SLACK $phase_path] < 0} {
        error "Missing or failing phase extraction timing in loop $channel"
    }
    puts $result "loop=$channel phase_extractor_setup=[get_property SLACK $phase_path] ns primitives=[llength $phase_cells]"
    report_timing -through $phase_pins -max_paths 4 -file $out/loop${channel}-phase.rpt
    foreach gain {gp gpi gi2 gi3} {
        set prefix "system_i/corrector$channel/inst/$gain"
        set cells [get_cells -hier -filter "NAME =~ $prefix/* && IS_PRIMITIVE"]
        if {[llength $cells] == 0} {error "Missing gain datapath $prefix"}
        if {[llength [filter $cells {REF_NAME == DSP48E1}]] != 0} {
            error "Expected table arithmetic in $prefix"
        }
        set pins [get_pins -hier -filter "NAME =~ $prefix/*"]
        set path [get_timing_paths -through $pins -max_paths 1 -no_report_unconstrained]
        if {[llength $path] != 1 || [get_property SLACK $path] < 0} {
            error "Missing or failing gain timing in $prefix"
        }
        puts $result "loop=$channel gain=$gain setup=[get_property SLACK $path] ns primitives=[llength $cells]"
        report_timing -through $pins -max_paths 4 -file $out/loop${channel}-${gain}.rpt
    }
}
set programmer [get_cells -hier -filter {NAME =~ system_i/gain_programmer/inst/* && IS_PRIMITIVE}]
if {[llength $programmer] == 0} {error "Missing gain programming interface"}
puts $result "programmer primitives=[llength $programmer]"
close $result
puts "Full table-gain instrument checks passed"
close_project
