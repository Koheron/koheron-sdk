# Temperature readouts

ALPHA15 signal analyzer, ALPHA250 FFT and ALPHA250-4 FFT share the template.
It retains the existing import name and data indices for the voltage reference,
board and Zynq readouts. Include `driver.mk` for the shared ALPHA15 and ALPHA250-4
RPC adapter, which supports Promise and callback reads.

Include `readout.mk` for the shared renderer. All three interfaces use one
decimal. The renderer avoids DOM writes when text is unchanged. Slow telemetry
uses the shared `web/instrument/poller.mk` lifecycle in ALPHA15; ALPHA250 and
ALPHA250-4 read telemetry through their FFT drivers once per second.
