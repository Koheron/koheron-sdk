# ALPHA15 anomaly detector

## Goal

Create a complete SDK instrument with a responsive Koheron-style web interface:
learn a normal signal, inject a disturbance, and inspect its capture.
Favor sensible defaults, clear behavior, and a simple, readable implementation.
Write the code as a teaching example: make the signal flow easy to follow,
use straightforward structures, and explain the reasoning behind key choices.

## Hardware and behavior

- **DAC0 is physically looped back to ADC0.** Generate a steady normal waveform
  and inject brief disturbances, using levels that avoid clipping.
- A small FPGA neural network predicts ADC0 from recent history. Absolute
  prediction error is the anomaly score. Process every 18-bit sample at
  15 MS/s with the lowest practical latency, without measurement windows.
- Train adaptively on recent ADC0 measurements while acquisition and detection
  continue; software training is allowed. Update the model without reloading
  the FPGA. Show when it updates and allow learning to pause and resume.
- An adjustable threshold drives a digital output directly from FPGA logic.
  Assert it when the score exceeds the threshold.
  Document the expansion pin and voltage. Detection uses ADC0 exclusively,
  without knowledge of disturbance injection.
- Safely reserve the full dedicated **128 MiB RAM** for a circular ADC buffer
  preserving all 18 bits. Capture **1 second before and 1 second after** each
  trigger, identifying its sample correctly.
- Retain one event until explicitly replaced. Detection and digital output
  continue during review. Report acquisition errors; never label damaged
  captures valid. Further anomalies must not automatically replace an event
  currently being acquired.

## Web experience

Serve one page from the board. After installation, operation needs no terminal
commands or external training tools.

1. **See the signal.** Show the live ADC waveform, explain the loopback connection,
   and start with a useful normal waveform and sensible settings.
2. **Learn normal.** One action collects data and trains, showing progress,
   then the live score and suggested adjustable threshold. Disable injection
   during learning.
3. **Detect.** One action prepares capture. Indicate when sufficient history
   exists and detection is ready; make **Inject anomaly** the obvious next step.
4. **Inspect.** Injection activates the digital output and produces a -1 s to
   +1 s waveform. Mark the trigger, preserve short events in the overview,
   support zoom, and show progress during posttrigger acquisition.
5. **Try again.** Make threshold adjustment, retraining, and another capture
   straightforward; preserve the current event until explicitly replaced.

Use Control's Koheron visual language. Show a clear diagram of the neural
predictor, measured and predicted live waveforms, live error, and statistics
for its features and fit. Keep the interface responsive during training and
capture verification. Use plain labels, clear status, and helpful errors.
Disable unavailable actions with explanations. Reopening restores board state.

## Done means

- Run on the looped-back board: demonstrate learning, normal operation without
  frequent false alarms, injected-anomaly detection, digital output, inspection,
  and repeated captures.
- Verify predictions against a software reference, continuous sample-rate
  operation, runtime model updates, and capture integrity across ring wrap.
  Evaluate fresh measurements beyond training data.
- Test the browser workflow, including zoom, reconnect, and errors. Brief
  anomalies remain visible in the full-capture view.
- Provide concise setup/use instructions and verification evidence. Report
  measured latency, limitations, and hardware checks left untested.
