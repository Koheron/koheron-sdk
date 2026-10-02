# Fix short DAC hold paths first. The following Explore pass recovers any setup
# slack consumed by the inserted delay buffers before the final timing gate.
set_clock_uncertainty -hold 0.020 -from [get_clocks -include_generated_clocks -of_objects [get_pins -hier *mmcm_adv*/CLKOUT0]] -to [get_clocks -include_generated_clocks -of_objects [get_pins -hier *mmcm_adv*/CLKOUT1]]
phys_opt_design -directive ExploreWithAggressiveHoldFix
