# FFT spectrum interface

Acquisition and DDS settings are above the spectrum. Board monitoring and
precision I/O expand below it.

- **View** switches between Spectrum, Spectrogram and Density. All three share
  the frequency zoom. Drag to zoom horizontally in a history view; double-click
  or Reset restores the full span. History is retained while switching views.
- **Average · 1 s** overlays a green exponential average of linear PSD with a
  one-second time constant. **Max hold** overlays the highest received power
  in each bin in amber. Both include received frames skipped by browser paint.
- **Spectrogram** shows frequency against age, with the newest row at the top.
  The waterfall scrolls continuously with each received frame, including
  fractional time-row movement; pause freezes that position. Each 50 ms row holds the strongest received power in each bin. Missing time
  slots stay blank, including pauses and hidden-tab gaps.
- **Density** shows frequency against level. Its color is the fraction of
  received spectra at that level, with a logarithmic occurrence scale that
  makes rare events visible. PSD is quantized into 255 levels from −200 to
  +20 dBm/Hz (about 0.87 dB per level); values beyond that range clamp to its
  endpoints. Frequency bins merged into a display pixel use the largest
  occurrence count, preserving narrow signals.
- **History** selects 5, 15 or 30 seconds for either history view. **Auto level**
  sets the color range in Spectrogram and level axis in Density; turn it off
  to enter Low and High in the selected unit. Changing units restores Auto
  level. Hover over either view to inspect its received data.
- **Clear history** resets both history views, average and max hold. Channel,
  window, sampling rate and reference clock changes also reset them. DDS
  changes retain history, so frequency changes remain visible. A captured
  reference is retained separately.
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
  and their acquisition settings. Enabled average and max-hold traces also
  appear in spectrum CSV files. History PNG exports show the selected view;
  history CSV exports contain all frequency bins, time rows with explicit
  missing slots for Spectrogram, or per-level occurrence counts for Density.
- Typed DDS and precision DAC values commit on Enter or leaving the field.
  Invalid values do not reach the instrument. Sliders update continuously.

Acquisition runs independently of drawing, targeting 60 spectra per second
with at most one request in flight. Each browser animation frame displays the
newest complete spectrum, up to 60 updates per second; older waiting frames
are replaced rather than queued. The header shows actual fresh-spectrum FPS.
Hover over it for the acquisition rate. Pause or hiding the tab suspends host
requests and drawing; the FPGA continues acquiring. Drawing preserves the minimum
and maximum in each screen column, retains missing-data gaps, and returns to
all bins when zoomed in. Board telemetry refreshes once per second; acquisition
controls continue to refresh four times per second. Captured references are
converted only on capture or unit changes. Measurements and exports retain
all FFT bins.

History describes spectra received by this browser, rather than continuous RF
capture. It retains at most 601 time rows (including the oldest partial row) and 2048 quantized spectra (30 seconds
at the target 60 Hz rate); older entries expire. Acquisition settings come from
the client's latest status, so this interface does not certify the exact FPGA
frame boundary of a hardware setting change.

Host regression instructions are in [tests/README.md](tests/README.md).
