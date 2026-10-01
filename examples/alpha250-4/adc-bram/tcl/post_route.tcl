# Close the marginal AXI BRAM setup paths without changing clocks or logic.
phys_opt_design -directive AggressiveExplore

# Vivado generates its timing report before invoking the post-route hook.
# Refresh it so the report describes the checkpoint used for the bitstream.
report_timing_summary -max_paths 10 -routable_nets -report_unconstrained \
    -file system_wrapper_timing_summary_routed.rpt \
    -pb system_wrapper_timing_summary_routed.pb \
    -rpx system_wrapper_timing_summary_routed.rpx -warn_on_violation
