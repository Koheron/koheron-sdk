# Shared clock adapter

Include `driver.mk` for the `ClockGenerator` RPC adapter, or `components.mk` for
the adapter and `ClockGeneratorApp` event bindings. Hosts supply their own clock
lifecycle. The adapter supports both callback and Promise forms of
`getReferenceClock`, plus the existing sample-clock selector and DAC-rate read.
Construction sends no commands.

ALPHA PNA, ALPHA15 and ALPHA250/ALPHA250-4 FFT share the event bindings. DPLL
uses only the adapter.
ALPHA250-4 reads its reference clock through its FFT control decoder; ALPHA15 uses the shared adapter's Promise reads. Board-specific
templates are shared through the selector-only includes below.
The optional third constructor argument runs before a clock write, allowing
ALPHA15 to invalidate acquisition before settings change. `dispose()` removes
the event bindings; construction only attaches listeners and sends no commands.

`tests/test_clock.cjs` checks RPC IDs, arguments, read contracts, errors, board
templates, acquisition notification order and listener disposal. It
runs once alongside the generic plot checks in `bash web/tests/run.sh`.

Include `reference-clock.mk` for the canonical internal/external 10 MHz
selector used by ALPHA15, ALPHA250/ALPHA250-4 FFT, both ALPHA PNAs and DPLL.
It retains command values 2 (internal) and 0 (external), wraps each radio in its
label and names the group for assistive technology. It adds no event bindings.
DPLL continues to own its asynchronous clock-change lifecycle.

Include `sampling-frequency.mk` for the identical 200/250 MHz selector used by
ALPHA250 and ALPHA250-4 FFT. ALPHA15 keeps its fixed 15 MS/s acquisition rate.

Clock bindings register their selectors with `InstrumentEvents`, included by
`components.mk`, and dispose that shared listener lifecycle on exit. The
settings-change callback still precedes the clock command. Driver-only consumers
continue to include `driver.mk` without UI dependencies.
