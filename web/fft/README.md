# Shared FFT interface

Include `components.mk` before board adapters and the entry point. It packages
one workspace, acquisition controls, spectrum/history views, exports and the
shared PNA generator widget. Mount `workspace.html` before control imports.

| Instrument | Board-specific behavior |
| --- | --- |
| ALPHA250 FFT | Clock selection, grouped telemetry and precision I/O |
| ALPHA250-4 FFT | Four inputs over two FFT engines; selected-pair sample rate; no RF DAC generator |
| ALPHA15 signal analyzer | Stitched LF/mid/RF voltage spectra, logarithmic Hz axis and input ranges; no RF DAC generator |
| Red Pitaya FFT | Fixed ADC rate and half-scale DAC presentation |

`FFTWorkspace` connects and initializes acquisition and controls without writing
DAC outputs. Generator discovery retries independently. Exit disposes controls,
plots and the client; a cached navigation return reloads to reconnect.

ALPHA250 and ALPHA250-4 share [board controls](../board-controls/README.md).
Slow telemetry uses [InstrumentPoller](../instrument/README.md), independently
of acquisition-control updates; ALPHA15 uses it in its local board controls.

[Instrument styles](../instrument/README.md) own common controls and headers;
`fft.css` owns the grid, responsive layout and 26-pixel control sizing.
[Precision channels](../precision-channels/README.md) own precision-editor styles.

Spectrum view has an optional **Persistence** toggle. Recent displayed live
traces fade with a 0.25–5 s decay time; intensity sets the strength of newly
added trails (5–50%). References, average/max hold, grid and cursors stay sharp.
Pause and hidden tabs freeze the trails. Clear trails removes only persistence;
Clear history, acquisition changes, units, zoom and resize clear it too.
Persistence follows displayed frames, so spectra skipped by painting are not
included. PNG includes the trails and settings; CSV retains full raw samples.

Try the real controls with simulated spectra and no hardware:

```sh
npm run preview:persistence --prefix web/plotting
# Open tmp/plotting/persistence-preview.html in a browser.
```

CSV/PNG filenames identify the instrument, board, history view when selected,
and UTC export time. PNG annotations wrap long names and acquisition labels at
the displayed plot width while preserving the backing canvas's full resolution.

See the [ALPHA250 interface guide](../../examples/alpha250/fft/README.md),
[ALPHA250-4 acquisition mapping and limitations](../../examples/alpha250-4/fft/README.md)
and [ALPHA15 acquisition guide](../../examples/alpha15/signal-analyzer/README.md).
Run `bash web/tests/run.sh fft` for shared/FFT checks or
`bash web/tests/run.sh alpha15` for shared/ALPHA15 checks; see the
[test guide](../../examples/alpha250/fft/tests/README.md).
