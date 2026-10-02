# ADC/DAC BRAM clock configurations

Build the example at its default 250 MHz. The same loaded FPGA can then run at
200, 250, 100, or 240 MHz using `set_sampling_frequency(0)`, `(1)`, `(2)`, or `(3)`.
No bitstream reload is needed between selections. Stop acquisition before
switching; the sample clock pauses during reconfiguration.

The driver changes the external clock and the MMCM dividers, waits for lock,
and applies the selected phase from zero. Re-selecting the current rate does
nothing. Rates above the build's `adc_clk` are rejected.

This change adds PS-domain MMCM control/status registers, so update the FPGA
and server together once. Build with `ENFORCE_TIMING=1` to enforce routed timing.
