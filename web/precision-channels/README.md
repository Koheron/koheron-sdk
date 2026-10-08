# Precision channels

Include `components.mk` after shared digit inputs for `PrecisionDac`,
`PrecisionChannelsApp` and `precision-channels.css`. Load the stylesheet before
host layout styles. `driver.mk` packages only the DAC adapter; `adc-driver.mk`
packages the ALPHA250-4 precision ADC adapter. DAC reads support callbacks and
Promises.

ALPHA15, ALPHA250 FFT and ALPHA250-4 FFT use the digit editor. Startup reads
settings; edits display millivolts, send volts and read back accepted values.
Telemetry preserves drafts and invalid entries. Dispose editors on exit.

Include `io-template.mk` for ALPHA250/ALPHA250-4's four-channel DAC/ADC table.
ALPHA15 retains its DAC-only template because it has no precision ADC.

Transport/editor and board-integration checks run through the
[browser runner](../tests/README.md).
