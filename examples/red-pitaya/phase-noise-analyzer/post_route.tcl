# Setup optimization can introduce short paths; fix hold before final checks.
phys_opt_design -hold_fix
route_design -preserve
# Vivado emits the step reports before the POST hook. Refresh them so the run
# statistics and the SDK's strict timing check describe the corrected design.
# The SDK top module is system_wrapper; the in-memory design may be design_1.
set design_name system_wrapper
report_timing_summary -max_paths 10 -report_unconstrained -warn_on_violation \
    -file ${design_name}_timing_summary_postroute_physopted.rpt \
    -pb ${design_name}_timing_summary_postroute_physopted.pb \
    -rpx ${design_name}_timing_summary_postroute_physopted.rpx
report_bus_skew -warn_on_violation \
    -file ${design_name}_bus_skew_postroute_physopted.rpt \
    -pb ${design_name}_bus_skew_postroute_physopted.pb \
    -rpx ${design_name}_bus_skew_postroute_physopted.rpx
