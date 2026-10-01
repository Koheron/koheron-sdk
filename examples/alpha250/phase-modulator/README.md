# ALPHA250 internal phase modulation

Two independent sine carriers with AXI-controlled internal phase modulation,
using the reusable [DDS PM IP](../../../fpga/ip/awg_v1_0/). Both channels use
48-bit phase accumulators, 16-bit DAC samples and the 250 MS/s board clock.
All internal PM sources are enabled in this demonstration. Outputs start muted.

## Build

From the SDK root, using the installed Vivado version:

```sh
make fpga CFG=examples/alpha250/phase-modulator/config.mk VIVADO_VERSION=2025.1 N_CPUS=4
make timing CFG=examples/alpha250/phase-modulator/config.mk VIVADO_VERSION=2025.1
make tmp/examples/alpha250/phase-modulator/serverd CFG=examples/alpha250/phase-modulator/config.mk N_CPUS=4 -j4
```

Timing enforcement is enabled. These commands build locally without deploying
or changing a board. Build the complete instrument with the usual `make` target
and install it separately before using the client.

## Generate a modulated carrier

```sh
.venv/bin/python examples/alpha250/phase-modulator/phase_modulator.py BOARD_IP
```

This enables OUT0 with a 10 MHz carrier, sine PM at 1 kHz and +/-30 degree
deviation. OUT1 remains muted. The carrier amplitude is full digital scale;
programmable output amplitude and DC offset are outside this first version.

For other settings, import the client next to your script:

```python
from koheron import connect
from phase_modulator import PhaseModulator, Waveform

pm = PhaseModulator(connect("BOARD_IP", name="phase-modulator"))
pm.configure(channel=0, carrier_hz=10_000_000, waveform=Waveform.SINE,
             modulation_hz=1_000, deviation="0.000001", restart=True)
pm.configure(channel=1, carrier_hz=5_000_000, waveform=Waveform.BPSK,
             modulation_hz=100_000, deviation=180, restart=True)
```

`deviation` and phase offsets accept strings or Decimal for precise conversion.
`configure_words` also accepts native phase words directly. BPSK alternates
two phase states at the symbol rate; PRBS is a separate bipolar PM source.
Set `pm_enabled=False` for an unmodulated tone or `output_enabled=False` to mute.
Changes with `restart=False` preserve oscillator state.

The example selects 250 MS/s through the ALPHA250 clock driver. If you change
the sampling clock through another driver, restore it before using this client;
the frequency conversion uses the example's configured sample rate.
Two independent commits do not synchronize channel start to the same sample.

The default design passes the SDK's strict constrained timing check at 250 MHz
with Vivado 2025.1 (setup margin 0.100 ns, hold margin 0.017 ns). See the IP
README for resource figures and the limits of the board I/O constraints.
No board deployment or physical PM measurement is included in offline tests.
