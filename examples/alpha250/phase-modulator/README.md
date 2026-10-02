# ALPHA250 internal phase modulation

Two independent sine carriers with AXI-controlled internal phase modulation,
contained in a single reusable [DDS PM IP](../../../fpga/ip/awg_v1_0/). Both
channels use 48-bit phase accumulators, 16-bit DAC samples and the 250 MS/s board clock.
The catalog instance uses `CHANNELS=2`; all internal PM sources are enabled
in this demonstration. Outputs start muted. Its single 8 KiB AXI region
contains channel banks at offsets `0x0000` and `0x1000`.

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

After installing the instrument, open the board's IP address in a browser.
The web interface reads the current configuration without changing outputs.
It provides one compact row per DAC, with extra settings behind **More**;
normal edits preserve oscillator phase. The interface uses the same visual
style as the ALPHA250 FFT workspace and adapts to narrow dashboard panels.
Its [shared widget](../../../web/phase-modulator/README.md) can be mounted in
other instrument pages with their existing Koheron client.

For carrier and PM rate, type a value such as `10 MHz` and press Enter, or click
a digit and scroll to tune that place. Left/Right selects a different digit;
Up/Down tunes it. Escape cancels typed entry and unsent tuning. The separate
unit selector also accepts Hz, kHz, MHz and GHz without changing the signal.

```sh
.venv/bin/python examples/alpha250/phase-modulator/phase_modulator.py BOARD_IP
```

This enables OUT0 with a 10 MHz carrier, sine PM at 1 kHz and +/-30 degree
deviation. OUT1 remains muted. The carrier amplitude is full digital scale;
programmable output amplitude and DC offset are outside this first version.

For other settings, import the client next to your script:

```python
from koheron import connect
from phase_modulator import PhaseModulator

pm = PhaseModulator(connect("BOARD_IP", name="phase-modulator"))
pm.tone(10_000_000)
pm.configure(waveform="sine", modulation_hz=1_000, deviation=30)
pm.set_frequency(12_000_000)
pm.set_deviation("0.000001")
print(pm.settings())
pm.mute()
pm.enable_output()             # Resume the same configuration
pm.restart()                   # Explicitly restart both oscillators

pm.configure(channel=1, carrier_hz=5_000_000, waveform="bpsk",
             modulation_hz=100_000, deviation=180)
print(pm.info(1))              # Precision, sample rate and available waveforms
```

`deviation` and phase offsets accept strings or Decimal for precise conversion.
Frequencies also accept strings or Decimal. Waveforms accept names or the
`Waveform` enum. Full `configure` enables output and PM and restarts both
oscillators by default; `tone` disables PM. Use `restart=False` to preserve
oscillator state during full configuration. Individual setters, mute and
enable operations preserve all other settings and do not restart oscillators.
They return the client object for chaining and raise a descriptive exception
when validation or a hardware commit fails.

`settings()` reads acknowledged native hardware settings and converts them
back to Decimal Hz, degrees and duty fraction. It can be passed to
`pm.configure(**pm.settings(), restart=False)` without losing native-word
precision. Capabilities are discovered once on connection; settings are read
from hardware. `configure_words` remains available for Boolean native-word
clients, and `configure_words_checked` returns an error message (empty on
success). BPSK alternates two phase states at the symbol rate; PRBS is a
separate bipolar PM source.

The command-line client also supports direct control and inspection:

```sh
.venv/bin/python examples/alpha250/phase-modulator/phase_modulator.py BOARD_IP --tone --carrier-hz 12000000
.venv/bin/python examples/alpha250/phase-modulator/phase_modulator.py BOARD_IP --channel 1 --waveform bpsk --modulation-hz 100000 --deviation 180
.venv/bin/python examples/alpha250/phase-modulator/phase_modulator.py BOARD_IP --mute
.venv/bin/python examples/alpha250/phase-modulator/phase_modulator.py BOARD_IP --status
.venv/bin/python examples/alpha250/phase-modulator/phase_modulator.py BOARD_IP --info
```

`--status` and `--info` leave the output untouched. `--no-restart` preserves
oscillator state during configuration. Rebuild and install the updated server
alongside this client to use the new RPC methods. The shared C++ driver offers
the same engineering-unit configuration, partial updates, mute, restart and
native settings readback; see the [IP driver example](../../../fpga/ip/awg_v1_0/README.md).

The example selects 250 MS/s through the ALPHA250 clock driver. If you change
the sampling clock through another driver, restore it before using this client;
the frequency conversion uses the example's configured sample rate.
Two independent commits do not synchronize channel start to the same sample.
The server reads the FPGA channel count and the Python client rejects absent
channels. For a single-channel instrument, set `CHANNELS 1` in the block design
and tie the unused board DAC input to zero. The IP then omits the second signal
path, and exposes only `dac0_data`.

The integrated two-channel design passes the SDK's strict constrained timing
check at 250 MHz with Vivado 2025.1: setup margin 0.000620 ns, hold margin
0.044454 ns, and eight checked bus-skew constraints. The setup margin is very
narrow; reroute and recheck after changing precision or included sources. See
the IP README for resource figures and the limits of the board I/O constraints.
The instrument was also installed and started on an ALPHA250. DAC0 accepted
a 10 MHz carrier with 10 kHz sine PM and 1 degree deviation; settings readback
confirmed output and PM enabled, with DAC1 muted. This verifies deployment,
metadata discovery and the live configuration/acknowledgement path. Analog PM
amplitude and spectrum have not yet been measured.

The web widget was checked in Chrome at desktop, dashboard-column and phone
widths. Live browser edits on muted DAC1 verified source selection, pulse duty
and carrier frequency; Enter commits retained keyboard focus. Exact native
readback confirmed DAC0 was unchanged, and the original DAC1 settings were
restored after the checks. Live frequency editing also verified direct unit
entry, selected-digit carry/borrow, wheel tuning, Escape cancellation and
display-only unit changes on muted DAC1. Fifteen DOM/protocol tests cover
widget behavior, digit tuning and empty-success response decoding.
