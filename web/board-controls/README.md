# ALPHA FFT board controls

Include `alpha-fft.mk` after the shared FFT components for ALPHA250 and
ALPHA250-4. `AlphaFFTControls` initializes the clock bindings and precision DAC
editor without writing outputs, exposes board controls and disposes listeners
and editors on exit. The host decoder supplies telemetry to the shared FFT
telemetry poller; precision-DAC readbacks preserve unfinished edits.

ALPHA250-4 passes a settings-change callback so clock edits invalidate pending
spectrum requests before sending the command. Its four-input decoder remains
board-specific. ALPHA15 keeps its input-range and multiband board controls.
