# DDS precision is chosen when generating the package. CHANNELS (1/2), PRBS
# order and source enables remain independently configurable on each instance.
# Change this dictionary and rerun make cores to build another precision profile.
set dds_pm_package_settings [dict create \
    PHASE_WIDTH 48 OUTPUT_WIDTH 16 MOD_WIDTH 24 LUT_BITS 14]
