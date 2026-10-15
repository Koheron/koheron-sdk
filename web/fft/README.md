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

The compact toolbar above the plot holds acquisition, view, units and plot actions.
Native spectrum snapshots can declare `powerBasis: 'full-scale'` when ADC input
calibration is unavailable. The display then shows **Uncalibrated** and offers
`dBFS/Hz` and `dBFS`; 0 dBFS is a full-scale sine at the ADC, independent of
analog input gain. Physical power/voltage units return with calibrated frames.
Snapshots retain their own basis through pause, history, references and exports.
References from a different basis remain saved and are hidden from the current
plot. The calibrated `read_psd` RPC keeps its watts/Hz meaning.

The toolbar wraps on smaller screens. The collapsible **Preferences** widget in the right
sidebar holds peak markers, Exclude DC, average/max hold and Clear history, plus
duration and level settings for history views. It also contains the optional
**Persistence** toggle, decay, intensity and Clear trails controls. It starts
collapsed, and its heading shows when persistence is on. Recent displayed live
traces fade with a 0.25–5 s decay time; intensity sets the strength of newly
added trails (5–50%). The live trace keeps its original blue; only the fading
trails use a muted blue-gray. References, average/max hold, grid and cursors stay sharp.
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
