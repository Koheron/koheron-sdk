# ALPHA FFT board controls

Include `alpha-fft.mk` after FFT components for ALPHA250 and ALPHA250-4.
`AlphaFFTControls.init()` binds clocks, reads precision DAC settings and exposes
board controls without writing outputs. Call `dispose()` on exit.

The FFT decoder supplies telemetry; DAC readbacks preserve unfinished edits.
ALPHA250-4 supplies a settings-change callback before clock writes and retains
its four-input decoder. ALPHA15 retains its range and multiband controls.
See [FFT composition](../fft/README.md) and [clock controls](../clock-generator/README.md).
