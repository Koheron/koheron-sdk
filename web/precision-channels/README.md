# Precision DAC controls

ALPHA15 signal analyzer, ALPHA250 FFT and ALPHA250-4 FFT include `components.mk`
after the shared digit inputs. It provides `PrecisionDac`, the
`PrecisionChannelsApp` editor and `precision-channels.css`. Load the stylesheet
after the base styles and before the host layout styles.

All three interfaces use the same validated digit editor, with explicit commits,
selected-digit tuning and readback. Startup only reads the DACs; edits send volts
and display millivolts. Telemetry preserves drafts and invalid edits. Dispose the
editor on exit to remove listeners and pending commits.

ALPHA250 and ALPHA250-4 include `io-template.mk` for the shared four-channel
DAC/ADC table. ALPHA15 supplies its DAC-only template because it has no precision
ADC. ALPHA250-4 feeds its ADC/DAC readbacks into the shared editor through the
shared FFT telemetry lifecycle; it no longer uses sliders or
animation-frame precision polling. The transport retains callback reads for
existing clients alongside Promise reads.

Run the transport/editor regressions through `bash web/tests/run.sh alpha15`
or any other browser suite. Tests exercise the shared template and ALPHA250-4/ALPHA15 workspaces, including read-only startup, invalid
input, millivolt/volt conversion, draft preservation and disposal.
