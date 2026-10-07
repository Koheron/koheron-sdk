# Precision DAC controls

ALPHA15 signal analyzer and ALPHA250 FFT include `components.mk` after the
shared digit inputs. It provides `PrecisionDac` and the `PrecisionChannelsApp`
editor. Board templates remain local so their ADC controls and labels do not
change. Startup only reads the DACs; edits send volts, display millivolts and
read back the returned setting. Telemetry preserves drafts and invalid edits.

The older ALPHA250-4 FFT includes only `driver.mk` and keeps its existing
slider widget and polling. The transport supports its callback read as well as
the Promise read used by the newer interfaces, with the same RPC commands.

Run the transport/editor regressions through `bash web/tests/run.sh alpha15`
or any other browser suite. The ALPHA15 workspace checks exercise the shared
components with its real adapters and four-channel board template.
