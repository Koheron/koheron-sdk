# Phase-noise web components

Include the appropriate component list before an instrument's local web assets:

- `components.mk`: spectrum framing, base plot, numeric editors, precision and
  sample-rate controls, connection-error presentation, CSV/PNG export engine and template, plot
  templates and common workspace styles.
- `analyzer/components.mk`: the common assets plus the single-stream driver,
  plot adapter and CSV/PNG export used by ALPHA250, Red Pitaya and the DPLL monitor.
- `analyzer/workspace.mk`: the single-stream components plus two-channel analyzer
  controls, oscillator template and embedded signal-generator styles and code.
- `analyzer/monitor.mk`: the single-stream components plus the passive monitor
  lifecycle, acquisition editors, polling and readouts used by DPLL.
- `reference-clock/components.mk`: the identical reference-clock driver, selector
  and bindings used by the two ALPHA PNA interfaces.
- `../clock-generator/driver.mk`: just the clock RPC adapter, also used by FFT
  and DPLL. Its reference-clock read supports callbacks and Promises. The shared
  selector lives under `web/clock-generator`; DPLL retains its connection lifecycle.

ALPHA250-4 includes `components.mk` and supplies its own cross-spectrum driver,
plot, export metadata and controls. Its local `phase-noise.css` adds cumulative averaging
styles after `workspace.css`. ALPHA250 and Red Pitaya load `generator.css` after
`workspace.css`. The DPLL monitor uses its own page and monitor layout. All four pages load
`instrument.css` for shared controls before their workspace styles; see
[`../instrument/README.md`](../instrument/README.md).

Keep board entry points, clock controls and board-specific templates in their
examples when they differ. Reusable assets belong here. The shared `PnaExportFile`
owns CSV rows, reference traces, PNG rendering and downloads; analyzer adapters
provide acquisition metadata and frame labels. The web packager flattens HTML/CSS asset
names, so component lists must avoid duplicate non-TypeScript basenames.

`PnaMonitor(document, client, driverName, onError)` owns the supplied connection,
initializes the single-stream driver, plot, precision and export controls, and
closes the connection on disposal. It uses the monitor DOM IDs in the DPLL page
and the even-rate 4–8192 CIC controls. All monitor RPCs use `driverName`. The DPLL
adapter supplies its separate ordered socket and `Dma` driver; feedback controls,
reference edits and their socket stay with the DPLL application.

Browser regression tests are in `tests/` and the consuming instruments' `tests/`
directories. Those fixtures load the shared sources directly; update their paths
when moving components.

Generic canvas-rendering checks live in `web/plot-basics/tests/` and run once in
the shared browser suite. Export regressions cover each consumer's metadata,
signed samples, reference grids and HiDPI PNG dimensions.

`bash web/tests/run.sh pna` runs the shared and PNA/DPLL browser suites
after the host runner's `cpp` stage has generated the spectrum payload fixture.
The [shared host runner](../../server/drivers/phase-noise/tests/README.md) adds
Python clients, sanitized C++ fixtures and compiled-payload integration checks.

`PnaSaveConfig` provides the same save-request feedback for ALPHA250, ALPHA250-4
and Red Pitaya. The existing RPC has no acknowledgement, so the UI reports
“Save requested”. Send errors show “Save failed” and use the host connection-error
handler. Repeated clicks reset one feedback timer; disposal removes the listener
and cancels that timer. The analyzer controls retain their board-specific RPCs.

`readPnaMeasurements` maps the measurement tuple while retaining each server's
float/double field precision and averaging argument. `PnaMeasurementReadout`
owns the common measurement presentation for
all three PNA pages and the DPLL monitor. Carrier power stays in dBm; phase and
time jitter convert radians/seconds to mrad/ps with RMS subscripts and two decimal
places. Missing readings show an em dash. Jitter-band endpoints use compact Hz
units, retain fractional values, and expose the exact Hz interval in a tooltip.
Disposal clears measurements and interval details. Board RPCs, integration bands
and measurement polling remain with each host.

`showPnaConnectionError` gives the three PNA pages the same disconnect message,
stale readouts and unavailable acquisition statuses. Each page still owns its
error guard, plot disposal, control disabling and connection shutdown.

`pnaIntegerInput` shares acquisition-number editing across PNA controls and the
DPLL monitor. Fields provide `min`, `max` and `step`; hosts supply commands and
accepted readbacks. All four interfaces use even CIC rates from 4 to 8192. Board averaging limits
and delay units remain in each host's field metadata and adapter.

The three PNA interfaces and the DPLL monitor share the FFT reference controls:
up to eight named captures, visibility toggles, recapture, removal, clear/undo,
and JSON save/load. The collection and panel live in `web/plot-references`;
instrument adapters retain their own sample format and file validation. Existing
FFT reference JSON files remain compatible with the FFT interfaces. PNA files
use a separate format and must match the board/monitor that captured them.

Each noise reference retains raw phase density, its acquisition settings and
capture timestamp. Changing the live sample rate, phase/frequency display or
smoothing preserves each reference's original frequency grid; ALPHA250-4 signed
cross spectra retain their sign. CSV and PNG include visible references; **Save
all** includes hidden references too. Recapture requires a valid live spectrum.
Reference conversions are cached between ordinary live updates.
