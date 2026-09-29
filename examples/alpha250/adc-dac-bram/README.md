# ADC/DAC BRAM sampling rate

Set `parameters.adc_clk` in `memory.yml` to 100000000, 200000000, 240000000,
or 250000000 Hz, then rebuild and load both the FPGA and server. The clock
driver starts at that rate. The default remains 250 MHz.

`set_sampling_frequency()` accepts only the rate matching the FPGA build;
other rates are rejected with a log message. Changing the external clock alone
does not reconfigure the MMCM or its timing constraints.

At 100 MHz, startup applies phase 300 automatically. Build with
`ENFORCE_TIMING=1` to require passing routed timing before writing the bitstream.
