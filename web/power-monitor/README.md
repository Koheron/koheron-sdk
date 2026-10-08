# Power monitor

`driver.mk` provides the ALPHA15/ALPHA250-4 callback/Promise RPC adapter and
unchanged Float32Array payload. Construction performs no reads or writes.
ALPHA250 FFT supplies telemetry through its FFT driver.

`components.mk` provides the supply template shared by all three boards;
`readout.mk` provides rendering that skips unchanged DOM text. Main voltage/current
use payload indices 1/0; clock voltage/current use 3/2. Voltages display in volts
with three decimals; currents display in milliamps with one.

Hosts own [telemetry polling](../instrument/README.md).
