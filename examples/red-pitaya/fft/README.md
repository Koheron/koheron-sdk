# Red Pitaya FFT

Build with `make CFG=examples/red-pitaya/fft/config.mk`. Requires a V1 OS image.

The 125 MS/s, 14-bit ADC instrument uses a 2048-point FFT. Its interface shares
`web/fft` with ALPHA250: the workspace, acquisition controls, plotting, history,
exports, styling and lifecycle. See the [FFT interface guide](../../alpha250/fft/README.md)
for controls and host regression instructions.

The design enables PS GP0 static ID remapping, reducing AXI transaction IDs
from 12 to 6 bits to simplify crossbar response tracking. This is appropriate
here because the instrument has only PS masters; disable it if a future PL
master accesses PL slaves through the PS. See the
[Processing System 7 guide (PG082)](https://docs.amd.com/api/khub/documents/5SmKudM_4jpvP8OtqGslWA/content).
The FFT arithmetic, clocks and memory map are unchanged.

With Vivado 2026.1 on the XC7Z010-1, this change improved routed worst setup
slack from -0.099 ns to +0.240 ns (zero failing setup or hold endpoints), and
reduced synthesis utilization from 15,301 to 15,028 LUTs and from 24,532 to
23,878 registers. These are results for this build, not a guarantee for other
tool versions or design changes. External ADC/DAC I/O delay constraints remain
incomplete, so passing the internal timing check is not full board I/O signoff.
Build with timing enforcement enabled:

```sh
make CFG=examples/red-pitaya/fft/config.mk VIVADO_VERSION=2026.1 ENFORCE_TIMING=1 fpga
```

Two DAC channels use the same DDS/PM IP, board driver and generator widget as
PNA. Outputs start muted. Carrier and PM settings use 48-bit native phase words;
carrier and modulation frequencies must stay below 62.5 MHz. The signed
16-to-14-bit conversion produces half-amplitude DAC output, matching PNA.
Opening the page only reads settings. The legacy FFT frequency RPC preserves
output enable and PM settings; use `PhaseModulator` to enable output.
