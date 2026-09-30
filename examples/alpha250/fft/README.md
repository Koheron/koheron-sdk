# FFT spectrum interface

Acquisition and DDS settings are above the spectrum. Board monitoring and
precision I/O expand below it.

- **Peak in view** reports the strongest finite bin in the live trace within
  the visible frequency range. Drag to zoom; Reset restores the full span.
  **Exclude DC** skips the zero-frequency bin, leaving other bins unchanged.
- **Pause** freezes the displayed frame while acquisition continues. Unit
  changes, zoom and peak search still work on the retained samples.
- **Capture ref** stores the full displayed spectrum as a purple reference.
  **Replace ref** captures a new one; **Clear ref** removes it. The reference
  retains its sample rate, window correction and channel when live settings
  change. Both traces convert to the selected unit using their own metadata.
  Reference cursor labels start with `Ref`. References last until page reload.
- **CSV** exports all bins, including those outside the zoomed view. When a
  reference is present, a second `Reference trace` section contains its own
  settings and frequency grid. **PNG** exports the current view, both traces
  and their acquisition settings.
- Typed DDS and precision DAC values commit on Enter or leaving the field.
  Invalid values do not reach the instrument. Sliders update continuously.

The display targets 20 updates per second, including acquisition and drawing
in its frame budget. Board telemetry refreshes once per second; acquisition
controls continue to refresh four times per second. Captured references are
converted only on capture or unit changes. Measurements and exports retain
all FFT bins.

Host regression instructions are in [tests/README.md](tests/README.md).
