# ALPHA15 streaming anomaly detector

Connect **DAC0 to ADC0** with a short coaxial cable. The board generates a
114.4 Hz triangle on DAC0 and can add a 1 µs pulse. ADC0 alone feeds a small
neural predictor, a live anomaly score, the trigger, and a 128 MiB DDR ring.
Open the board's web page to start adaptive learning, arm, inject and inspect a capture.

## Build and run

Use an ALPHA15 SD image built from this SDK revision. Its `/dev/cma` driver
supports reserving the board's dedicated 128 MiB pool; older images cannot
run this example safely. Build the standard image with
`make image CFG=examples/alpha15/signal-analyzer/config.mk` if needed.

From the SDK root, build and install the instrument:

```sh
make CFG=examples/alpha15/anomaly-detector/config.mk
make run CFG=examples/alpha15/anomaly-detector/config.mk HOST=192.168.1.105
```

Then open `http://192.168.1.105/` in a browser. Use the board's actual IP
address. Press **Start adaptive learning**. The first model appears after about
one second of ADC history; new fits use recent measurements while acquisition
and detection continue. The page shows measured and predicted waveforms, live
error, holdout statistics, eight feature weights and their mean contributions.
Press **Prepare capture**, then **Inject 1 µs pulse**. The board retains the
event until **Replace held capture** is pressed. Reopening the page restores
board state. Learning can be paused and resumed. A held event must be replaced
before new data can be fit.

The anomaly output is **B35_0_N**, package pin **J14 / AD6N**, 3.3 V LVCMOS.
It follows the threshold comparison directly in FPGA logic. Check the external
circuit's voltage and ground before attaching it.

## Signal path

- The DAC waveform and injection are generated in the 240 MHz DAC clock domain.
  The injection command only changes DAC0. The detector receives ADC0 and no
  injection signal.
- At every 15 MS/s ADC conversion, four previous 18-bit samples feed eight
  fixed ReLU units: positive and negative halves of each signed two's-complement
  delay.
  A background thread repeatedly fits the eight output weights and bias by
  ridge regression on the first 750 ms of a recent one-second recording. The
  last 250 ms is held out to report MAE, RMSE, and 99.9% error and suggest a
  threshold. Sampling the fit every 256 conversions keeps CPU use bounded;
  acquisition and the live page continue during fitting. Coefficients are
  quantized to Q12 and committed through control registers. A brief FPGA
  settling guard prevents mixed old/new pipeline values from asserting the
  alert for 32 fabric clocks (133 ns) per update. The adjustable suggested
  threshold has a 3,000-count floor: rare loopback spikes crossed lower
  thresholds in board tests. No FPGA reload is involved.
- The absolute one-step prediction error is the score. The score and digital
  output update two 240 MHz clocks after the ADC-valid edge (8.33 ns in RTL,
  before I/O propagation). There is no measurement window or decimation.
- AXI DMA writes one 32-bit word per ADC sample: bits 17:0 are the complete
  sample, bits 31:18 are a rolling conversion tag. The 128 MiB ring holds
  33,554,432 samples (2.237 s). Each DMA descriptor covers 131,072 samples.
  The raw ADC words are two's-complement; plots convert them to signed values
  without changing the recorded bits. A service thread rearms descriptors.
- The driver uses the board-pool `/dev/cma` ioctl for the entire dedicated
  128 MiB pool and requires its physical base to be `0x18000000` before
  enabling DMA. It reads that RAM through the SDK's uncached `/dev/mem`
  mapping. An unavailable or fragmented pool becomes a visible acquisition
  error.
- After the first full second, the FPGA may trigger. It records the trigger
  sample sequence, keeps the following second, and stops at the next full DMA
  packet. The driver checks all 30 million event tags and overflow/error flags
  before marking the event valid. It builds the full-event min/max overview
  during that scan, outside the state lock, keeping the interface responsive.
  More anomalies cannot replace a pending or held event. The score and output
  continue while the event is inspected.

The live plot shows about two DAC periods with a software reference of the
current FPGA model overlaid. A separate plot shows absolute error and threshold.
The event overview uses min/max bins and marks the trigger without averaging
away short pulses. Click it to zoom; the 100 µs view shows the pulse shape.

## Verification

Run the RTL reference test with Vivado Simulator:

```sh
/tools/Xilinx/2025.1/Vivado/bin/xvlog -nolog \
  examples/alpha15/anomaly-detector/cores/anomaly_engine_v1_0/anomaly_engine.v \
  examples/alpha15/anomaly-detector/tests/anomaly_engine_tb.v
/tools/Xilinx/2025.1/Vivado/bin/xelab -nolog anomaly_engine_tb -s anomaly_engine_tb_sim
/tools/Xilinx/2025.1/Vivado/bin/xsim anomaly_engine_tb_sim -R -nolog
```

It checks a known prediction, anomaly pulse, guarded live weight change, two-clock
alert delay, and packet-aligned trigger. Run the end-to-end loopback check
after installing the instrument:

```sh
PYTHONPATH=python .venv/bin/python3 \
  examples/alpha15/anomaly-detector/tests/verify_board.py 192.168.1.105
```

It checks measured loopback amplitude, asynchronous adaptive updates during
detection, live trace and holdout error, quiet normal operation (10 s by
default; use `--normal-seconds 60` for a longer run), three injected events,
runtime retraining, each trigger against a software prediction, sample tags
near each trigger, and the physical DDR wrap. The board scans all 30 million
retained tags before reporting an event as valid.

On the looped-back board, the adaptive model updated 70 times in a 60 s run
without a false trigger at the 3,000-count threshold. A separate 60 s
end-to-end run also stayed quiet, then verified three injected events, including
a physical DDR wrap and retraining after capture replacement. The injected
trigger errors were 30,055, 16,482, and 7,446 counts; the holdout 99.9% error
was 46 counts. All three 30-million-sample tag scans passed. Routed timing
passed with +0.156 ns setup slack. The browser workflow was checked at desktop
and 390 px mobile widths, including capture zoom and page reopening.
Physical J14 voltage and board-level I/O delay still require an external probe.
