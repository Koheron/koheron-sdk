# Temperature readouts

ALPHA15 signal analyzer and ALPHA250/ALPHA250-4 FFT share the template and
payload indices for voltage-reference, board and Zynq temperatures.
`driver.mk` provides the ALPHA15/ALPHA250-4 callback/Promise RPC adapter;
`readout.mk` provides one-decimal rendering that skips unchanged DOM text.

Hosts supply telemetry through [InstrumentPoller](../instrument/README.md),
either in board controls or their FFT decoder.
