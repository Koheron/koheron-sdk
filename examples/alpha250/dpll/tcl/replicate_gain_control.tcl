# Table programming and applied-bank signals fan out to distributed RAM bits. Force
# physical copies of their existing launch registers after placement, even
# when the estimated slack is positive. This adds no programming or loop delay.
set sources [get_cells -hier -filter {REF_NAME == FDRE && NAME =~ *gain_programmer/inst/command*_reg*}]
if {[llength $sources] < 18} {error "Expected both nine-bit gain programming commands"}
set data_sources [get_cells -hier -filter {REF_NAME == FDRE && NAME =~ *gain_programmer/inst/data_reg*}]
if {[llength $data_sources] < 64} {error "Expected the 64-bit gain programming payload"}
set bank_sources [get_cells -hier -filter {REF_NAME == FDRE && NAME =~ *gain_programmer/inst/active_banks_reg*}]
if {[llength $bank_sources] < 8} {error "Expected eight applied gain banks"}
set sources [concat $sources $data_sources $bank_sources]
set nets [get_nets -of_objects [get_pins -of_objects $sources -filter {REF_PIN_NAME == Q}]]
set_property FORCE_MAX_FANOUT 16 $nets
phys_opt_design -force_replication_on_nets $nets

# Replicated registers retain the same two-clock programming protocol.
source [file normalize [file join [file dirname [info script]] gain_programming_timing.tcl]]

# Phase and state registers also address wide distributed tables. Their small
# logical fanout can span both clock columns; create local launch copies rather
# than adding another sample of delay to the control loop.
set frequency_sources [get_cells -hier -filter {REF_NAME == FDRE && NAME =~ *phase_unwrapper/inst/*unwrapped_diff_reg*}]
if {[llength $frequency_sources] < 50} {error "Expected both 25-bit frequency launch registers"}
set loop_sources [get_cells -hier -filter {REF_NAME == FDRE && (NAME =~ *phase_unwrapper/inst/*unwrapped_diff_reg* || NAME =~ *consumers/inst/feedback_phase_reg* || NAME =~ *reference_pipeline.fast_i_phase_reg* || NAME =~ *accurate_controller/fused.acc2_reg* || NAME =~ *accurate_controller/acc1_reg* || NAME =~ *selector/active_fast_reg* || NAME =~ *detector/phase_reg*)}]
set loop_nets [get_nets -of_objects [get_pins -of_objects $loop_sources -filter {REF_PIN_NAME == Q}]]
set_property FORCE_MAX_FANOUT 8 $loop_nets
phys_opt_design -force_replication_on_nets $loop_nets

# Each accurate gain result feeds separate summing/accumulating states. Keep
# launch copies beside those consumers; the gain's register count is unchanged.
set gain_result_sources [get_cells -hier -filter {REF_NAME == FDRE && NAME =~ *accurate_controller/*/tables.*.P_reg*}]
if {[llength $gain_result_sources] < 200} {error "Expected both accurate gain result buses"}
set gain_result_nets [get_nets -of_objects [get_pins -of_objects $gain_result_sources -filter {REF_PIN_NAME == Q}]]
set_property FORCE_MAX_FANOUT 2 $gain_result_nets
phys_opt_design -force_replication_on_nets $gain_result_nets

# The ADC reset and integrator enables reach independent control and monitoring
# blocks. Keep their existing synchronous semantics with local physical copies.
set control_sources [get_cells -hier -filter {REF_NAME == FDRE && (NAME =~ *rst_adc_clk*FDRE_PER_N || NAME =~ *axi_ctl_register/inst/write_decode.word*regs_flat_reg*)}]
set control_nets [get_nets -of_objects [get_pins -of_objects $control_sources -filter {REF_PIN_NAME == Q}]]
set high_control_nets {}
foreach net $control_nets {
    if {[llength [get_pins -leaf -of_objects $net -filter {DIRECTION == IN}]] > 32} {lappend high_control_nets $net}
}
set_property FORCE_MAX_FANOUT 32 $high_control_nets
phys_opt_design -force_replication_on_nets $high_control_nets
