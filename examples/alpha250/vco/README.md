# ALPHA250 VCO

Two voltage-controlled oscillators: ADC0 controls DAC0's frequency, and ADC1
controls DAC1's frequency. Each output is a sine wave with a programmable
carrier offset. The design uses 48-bit DDS phase increments and a fixed
250 MS/s sampling clock.

## Build and run

Use a V0 OS image on the board. From the SDK root:

```sh
make CONFIG=examples/alpha250/vco/config.yml
make run CONFIG=examples/alpha250/vco/config.yml HOST=192.168.1.100
HOST=192.168.1.100 .venv/bin/python examples/alpha250/vco/test.py
```

The script selects the onboard TCXO reference and sets channel 0 to a 10 MHz
carrier offset with gain 4. Connect a control voltage to ADC0 and observe DAC0
with an oscilloscope or spectrum analyzer, respecting the board's input limits.
With the ADC input near zero, the output is near 10 MHz. The output keeps
running after the script exits. This is a usage example, not an automated
measurement test.

## Controls

- `set_dds_freq(channel, freq_hz)` sets the carrier offset from 0 to 125 MHz.
- `set_vco_gain(channel, gain)` sets a right-shift count from 0 to 48. Zero
  gives maximum sensitivity; each increment halves the ADC contribution.
  It controls frequency sensitivity, not output amplitude.
- Channels are numbered 0 and 1. Invalid channels, out-of-range values and
  non-finite frequencies are ignored without changing registers.

For signed ADC sample `a`, the frequency contribution is approximately
`a * 250e6 / (65536 * 2**gain)` Hz, with integer quantization in the FPGA.
The DDS phase increment wraps modulo 48 bits, so choose the offset and gain
to keep the modulated frequency within the desired band. Gain 48 gives the
smallest contribution; it is not an output mute.

Keep the sampling clock at 250 MS/s so the frequency conversion remains valid.
For an external reference, connect it first and change
`set_reference_clock(2)` to `set_reference_clock(0)` in the script. Importing
`test.py` only defines the client; it does not connect to a board.
