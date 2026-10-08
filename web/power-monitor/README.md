# Power monitor adapter

Include `driver.mk` for the shared ALPHA15 signal analyzer and ALPHA250-4 FFT
RPC adapter. `getSuppliesUI` supports Promise and callback reads and returns the
unchanged Float32Array payload. Construction performs no reads or writes.
Polling remains with each instrument.

Include `components.mk` for the shared supply readout template used by ALPHA15,
ALPHA250 FFT and ALPHA250-4 FFT. Main voltage/current use payload indices 1/0;
clock voltage/current use indices 3/2. Values display in volts and milliamps.
The template retains its `power-monitor.html` import name and `.supply-span`
selectors. Include `readout.mk` for the shared renderer, which displays volts
with three decimals and milliamps with one. It avoids DOM writes when text is
unchanged. ALPHA250 FFT reads telemetry through its FFT driver and uses the
shared renderer and template without the separate RPC adapter.
