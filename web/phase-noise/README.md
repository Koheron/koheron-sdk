# Phase-noise web components

Include the appropriate component list before an instrument's local web assets:

- `components.mk`: spectrum framing, base plot, numeric editors, precision and
  sample-rate controls, DDS adapter, CSV/PNG export engine and template, plot
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
  and DPLL. Its reference-clock read supports callbacks and Promises; the DPLL clock template
  and connection lifecycle remain local.

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
