# Shared clock adapter

Include `driver.mk` for the `ClockGenerator` RPC adapter, or `components.mk` for
the adapter and `ClockGeneratorApp` event bindings. Hosts supply their own clock
templates and lifecycle. The adapter supports both callback and Promise forms of
`getReferenceClock`, plus the existing sample-clock selector and DAC-rate read.
Construction sends no commands.

ALPHA PNA and ALPHA250 FFT share the event bindings. DPLL uses only the adapter.
The older ALPHA250-4 FFT retains its own reference polling, and ALPHA15 retains
its asynchronous board adapter. Board-specific templates stay with their hosts.

`tests/test_clock.cjs` checks RPC IDs, arguments, read contracts and errors. It
runs once alongside the generic plot checks in `bash web/tests/run.sh`.
