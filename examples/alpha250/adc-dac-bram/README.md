# ALPHA250 ADC/DAC BRAM

Connect DAC0 to ADC0 and DAC1 to ADC1 for loopback validation. Use a V1 OS image.
The DAC waveform and each two-channel ADC capture contain 65536 sample times.

`get_adc()` triggers a fresh capture and returns only after the FPGA has written
the complete buffer. The FPGA starts at the next shared DAC/ADC address wrap;
its completion flag toggles when the final ADC sample is written. The driver
waits for a change from the pre-trigger flag, so an earlier completed capture
cannot satisfy a new request. `trigger_acquisition()` also waits for completion.
Existing RPC signatures and the packed ADC array format are unchanged.

Clock changes are excluded until capture/readout finishes. With responsive MMIO,
a capture that fails to complete within 20 ms returns a failed checked result
instead of stale data. The Python client uses `get_adc_checked()` when available
and raises an error after consuming the complete reply, keeping RPC framing
intact. Legacy `get_adc()` returns zeros and logs the timeout. The
usual boundary wait plus capture takes at most 0.53 ms at 250 MS/s or 0.66 ms
at 200 MS/s, before polling and readout overhead.

Version 0.2.2 requires the matching rebuilt bitstream and server; the additional
status register is not available in older FPGA designs.

```sh
bash examples/alpha250/adc-dac-bram/tests/run-fpga.sh
.venv/bin/python3 -m unittest discover -s server/tests -p test_bram_capture.py
make CFG=examples/alpha250/adc-dac-bram/config.mk all N_CPUS=4
make CFG=examples/alpha250/adc-dac-bram/config.mk HOST=BOARD_IP deploy
```

For hardware validation, run `validate_loopback.py` with both loopback cables:
it checks both inputs at 200 and 250 MS/s, including every first capture after
replacing a DAC waveform. FPGA/host checks and hardware results are separate.
