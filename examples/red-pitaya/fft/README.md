# Red Pitaya FFT

Build with `make CFG=examples/red-pitaya/fft/config.mk`. Requires a V1 OS image.

The 125 MS/s, 14-bit ADC instrument uses a 2048-point FFT. Its interface shares
`web/fft` with ALPHA250: the workspace, acquisition controls, plotting, history,
exports, styling and lifecycle. See the [FFT interface guide](../../alpha250/fft/README.md)
for controls and host regression instructions.

Two DAC channels use the same DDS/PM IP, board driver and generator widget as
PNA. Outputs start muted. Carrier and PM settings use 48-bit native phase words;
carrier and modulation frequencies must stay below 62.5 MHz. The signed
16-to-14-bit conversion produces half-amplitude DAC output, matching PNA.
Opening the page only reads settings. The legacy FFT frequency RPC preserves
output enable and PM settings; use `PhaseModulator` to enable output.
