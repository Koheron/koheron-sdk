# DAC transfer timing

Run this check on the **final routed checkpoint**, in addition to the normal
whole-design timing gate (`ENFORCE_TIMING=1`):

```sh
vivado -mode batch -source boards/alpha250/tests/check_dac_timing.tcl \
  -tclargs path/to/final-routed.dcp path/to/reports
```

For `adc-dac-bram`, use `system_wrapper_postroute_physopt.dcp` from `impl_1`.
For designs without post-route physical optimization, use
`system_wrapper_routed.dcp`.

The 250 MS/s driver applies 56 fine-phase increments to MMCM CLKOUT0 after
loading the bitstream. Each increment is 1/56 of the VCO period (AMD UG472).
The script checks both the bitstream phase and that operating phase. It changes
`CLKOUT0_PHASE` only in the in-memory timing model, allowing Vivado to rederive
the generated clocks while preserving the complete clock paths. It does not
write a checkpoint or bitstream and does not program hardware.

The board XDC reserves 1 ns of setup time at 250 MS/s using additional clock
uncertainty. This bounds setup for phases 0 through 56 while retaining the
phase-0 hold requirement. During this explicit phase check, the script removes
that reserve from the in-memory model to avoid counting the shift twice.
Vivado's calculated jitter and MMCM phase error remain active.

Both setup and hold must have finite, nonnegative slack. Reports include the
processing-clock to DAC-clock paths for both MMCM input-clock selections.
The script rejects unsupported clock configurations and exits with an error
when a check fails.

This is an internal DAC transfer check for 250 MS/s. It does not characterize
arbitrary runtime phases, other sample rates, ADC input timing, or timing
between the FPGA pins and the external DAC. Repeat hardware validation when
changing the DAC clock distribution or the external clock-generator delay.
