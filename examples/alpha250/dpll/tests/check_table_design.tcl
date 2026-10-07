if {$argc != 2} {error "Expected full-instrument project and report directory"}
set project [file normalize [lindex $argv 0]]
set out [file normalize [lindex $argv 1]]
file mkdir $out
open_project -read_only $project
if {[get_property top [current_fileset]] ne "system_wrapper"} {
    error "The instrument wrapper must be the synthesis top"
}
open_bd_design [get_files */system.bd]
foreach channel {0 1} {
    foreach {name expected} {FUSED 1 GAIN_STAGES 4 FAST_GAIN_STAGES 3 TAIL_GAIN_STAGES 4 FINAL_CSA_LEVELS 2 CARRY_BLOCK 0 FAST_P_DSP 1 PIPELINED_REFERENCE 1 PRECOMBINE_I 1 SELECTOR_CARRY_BLOCK 0 PHASE_FRAC 8 FREQ_WIDTH 25 PHASE_WIDTH 40} {
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
    foreach gain {gp gpi gi2 gi3} {
        set prefix "system_i/corrector$channel/inst/accurate_controller/$gain"
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
    foreach block {fast_gain fast_i_gain detector selector} {
        set prefix "system_i/corrector$channel/inst/$block"
        set cells [get_cells -hier -filter "NAME =~ $prefix/* && IS_PRIMITIVE"]
        if {[llength $cells] == 0} {
            error "Missing manual P datapath $prefix"
        }
        set dsps [llength [filter $cells {REF_NAME == DSP48E1}]]
        if {($block eq "detector" && $dsps != 4) ||
            ($block eq "fast_gain" && $dsps != 2) ||
            ($block ni {detector fast_gain} && $dsps != 0)} {
            error "Unexpected DSP count in $prefix: $dsps"
        }
        set pins [get_pins -hier -filter "NAME =~ $prefix/*"]
        set path [get_timing_paths -through $pins -max_paths 1 -no_report_unconstrained]
        if {[llength $path] != 1 || [get_property SLACK $path] < 0} {
            error "Missing or failing manual P timing in $prefix"
        }
        puts $result "loop=$channel block=$block setup=[get_property SLACK $path] ns DSPs=$dsps"
        report_timing -through $pins -max_paths 4 -file $out/loop${channel}-${block}.rpt
    }
}
set programmer [get_cells -hier -filter {NAME =~ system_i/gain_programmer/inst/* && IS_PRIMITIVE}]
if {[llength $programmer] == 0} {error "Missing gain programming interface"}
puts $result "programmer primitives=[llength $programmer]"
set programming_sources [filter $programmer {REF_NAME == FDRE && (NAME =~ *data_reg* || NAME =~ *command*_reg*)}]
set held_sources {}
set strobe_sources {}
foreach source $programming_sources {
    if {[regexp {/(data_reg\[|command[01]_reg\[[0-7]\])} $source]} {lappend held_sources $source}
    if {[regexp {/command[01]_reg\[8\]} $source]} {lappend strobe_sources $source}
}
set memories [get_cells -hier -filter {IS_PRIMITIVE && (REF_NAME =~ RAMD* || REF_NAME =~ RAMS*) && (NAME =~ *tables.chunk*.products_reg* || NAME =~ *dsp_p.coefficients_reg*)}]
set write_pins [get_pins -leaf -of_objects $memories -filter {REF_PIN_NAME =~ WADR* || REF_PIN_NAME =~ ADR* || REF_PIN_NAME == I || REF_PIN_NAME == WE}]
set bank_sources [filter $programmer {REF_NAME == FDRE && NAME =~ *active_banks_reg*}]
if {[llength $held_sources]<80 || ![llength $write_pins]} {error "Missing held programming paths"}
foreach {kind sources targets expected} [list held $held_sources $write_pins 8.0 strobe $strobe_sources $write_pins 4.0 bank $bank_sources {} 4.0] {
    if {![llength $sources]} {error "Missing $kind programming sources"}
    if {[llength $targets]} {
        set path [get_timing_paths -from $sources -to $targets -max_paths 1 -no_report_unconstrained]
        report_timing -from $sources -to $targets -max_paths 3 -file $out/programming-$kind.rpt
    } else {
        set path [get_timing_paths -from $sources -max_paths 1 -no_report_unconstrained]
        report_timing -from $sources -max_paths 3 -file $out/programming-$kind.rpt
    }
    if {[llength $path]!=1 || abs([get_property REQUIREMENT $path]-$expected)>0.001} {
        error "Incorrect $kind programming timing constraint"
    }
    puts $result "programming=$kind requirement=[get_property REQUIREMENT $path] ns setup=[get_property SLACK $path] ns"
}
close $result
puts "Full table-gain instrument checks passed"
close_project
