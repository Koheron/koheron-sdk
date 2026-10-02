# DDS phase-modulator widget

A compact signal-generator control for one or two DAC channels, styled like
the ALPHA250 FFT workspace. Each channel has output enable, carrier frequency,
PM enable/source/rate and phase amplitude. **More** reveals carrier phase,
pulse duty, noise/PRBS seed and an explicit phase restart. Layout follows the
widget container's width, so it also fits a sidebar or dashboard panel.

## Embed

Add these shared assets to the instrument's `config.mk`:

```make
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator.ts
WEB_FILES += $(SDK_PATH)/web/phase-modulator/frequency-input.ts
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator-widget.ts
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator.css
```

Load `phase-modulator.css` in the host page and provide a mount element:

```html
<link rel="stylesheet" href="phase-modulator.css">
<div id="generator" class="dds-pm-widget" style="--pm-loading-channels: 2">
  <div class="pm-initial-layout" aria-hidden="true"></div>
</div>
```

After the host application's shared Koheron `Client` is initialized:

```typescript
const driver = new PhaseModulatorDriver(client);
const generator = new PhaseModulatorWidget(document.getElementById('generator'), driver,
    {expectedChannels: 2});
await generator.init();

// During host teardown:
generator.dispose();
```

The widget owns neither the socket pool nor the page lifecycle. The host calls
`client.exit()` when its application closes. All DOM access stays inside the
mount element, CSS is scoped to `.dds-pm-widget`, and there are no generated
global IDs, imports or polling loops. A frequency editor temporarily captures
page wheel events while focused, and releases that listener on blur or disposal. Multiple
widgets can share a client and transport adapter. The host can listen for the
bubbling `dds-pm-ready` event to update its connection indicator.

The loading placeholder reserves the same responsive layout as the controls.
Set `expectedChannels: 1` and `--pm-loading-channels: 1` when embedding a
single-channel build (default: two);
hardware discovery still determines which channels are available. For a host
that connects before constructing the widget, render
`PhaseModulatorWidget.loadingMarkup(2)` inside a `.dds-pm-widget` mount first.
Acknowledgement/error messages use a reserved status line, and focus hints
are positioned without changing row height. Long status messages scroll
horizontally inside that line. The supplied Lato font faces use optional font
loading to avoid a late swap; hosts may preload the two supplied WOFF2 weights.

The supplied adapter uses the example's `PhaseModulator` RPC interface;
its optional second constructor argument selects another RPC class name.
For a project with a different driver, implement `PhaseModulatorPort`
(`init`, `settings`, `set`, `restart`) and inject that adapter instead. Keep
clock selection in the host application and report its actual sample rate.
No board-specific clock or DAC logic lives in the widget.

## Editing behavior

Opening, retrying initialization and refreshing only read hardware. Numeric
edits commit on Enter or blur; typing alone sends no commands. Output and PM
toggles and source selection apply immediately. Rate and amplitude can be
prepared while PM is off. Pulse duty and seed controls follow the selected
source and hardware capabilities.

Amplitude, carrier phase, duty and seed also support F2 to select the value and
Escape to restore the last accepted setting while retaining focus. Enter applies
once; a subsequent blur does not repeat the write. Readbacks preserve unfinished
entries. Validation messages remain in the reserved channel status line after
focus moves, and clear when that entry is corrected or cancelled; unrelated
edits cannot hide them. Transport failures remain visible alongside draft errors.

Carrier and PM rate use a reusable digit editor:

- Type a number in the selected unit, or include a suffix such as `12.5 MHz`,
  `10000 Hz` or `15k`. Scientific notation is accepted. Enter or leaving the
  control applies the value. F2 or Ctrl/Command+A selects the whole value.
  Escape discards typed entry and unsent tuning, clears digit selection and
  restores normal page scrolling while retaining keyboard focus.
- Click a digit, then scroll or press Up/Down to change that place value.
  Left/Right selects the adjacent place, skipping separators. Right can reveal
  finer decimal places down to the hardware's resolution.
- The highlighted place and tuning step survive carries, borrows and unit
  changes: a 1 kHz step takes `9.999 MHz` to `10.000 MHz`.
- The unit selector changes display units without a hardware write. For typed
  entry, selecting a unit finishes the number in that unit.
- Wheel tuning follows the focused input with one digit selected, even when
  the pointer is elsewhere on the page. Clicking another control ends tuning.
  Without an active digit, wheel events scroll normally; small trackpad deltas
  accumulate into a step. Ctrl/Command-wheel remains available for browser zoom.
  Unfinished trackpad gestures reset when selecting another digit or pausing.

The focused editor shows its tuning step without adding a permanent toolbar.
Fast tuning coalesces unsent values and sends at most ten tuning requests per
second. Carrier and rate operations serialize within each channel. Failed or
ambiguous commits cancel queued tuning; they are never retried automatically.
Escape and disposal cannot retract a command already sent to hardware.
Acknowledgements preserve whole-value selection and the caret, and frequency
readouts retain full contrast during continuous tuning. Fractional tuning steps
use mHz/µHz in the focus hint.

Every edit uses a checked server setter that reads/modifies/commits under the
channel lock. Other settings, including native phase words, are preserved.
Oscillators keep running through edits and mute; **Restart phase** is explicit.
Other controls on the channel wait for acknowledgement; frequency editors
remain responsive and accumulate tuning while the request is pending.
The widget reads accepted settings after each operation and displays failures
inline. Refresh never retries a failed write. Closing the widget removes its
listeners and ignores late responses; an already-issued command can still
complete in hardware.

The adapter decodes 48-bit native readback exactly as pairs of uint32 words.
The UI displays selectable Hz/kHz/MHz/GHz, degrees and percent duty. Frequency
digits are grouped and retain decimal places for tuning; values within half a
hardware LSB are normalized to hide quantization artifacts such as
`9.9999999996 kHz`. Hover over a numeric field for its accepted value.
The Python native-word/Decimal API remains available for exact scripted work.
Output amplitude is full scale; the amplitude field controls phase in degrees.

See the [ALPHA250 example](../../examples/alpha250/phase-modulator/web/index.html)
for a minimal host page and application bootstrap.

## Checks

```sh
npm install --prefix web
NODE_PATH=web/node_modules node --test examples/alpha250/phase-modulator/tests/test_web_widget.js
make web CFG=examples/alpha250/phase-modulator/config.mk
```

The tests exercise real DOM events and the SDK tuple decoder: read-only opening,
multiple instances, one-channel/reduced-source builds, range and seed checks,
unit conversion, partial edits, mute/resume/restart, pending-operation locking,
timeouts, retry, disposal and 48-bit native readback. Frequency checks cover
keyboard entry, digit carry/borrow, unit selection, focused wheel tuning,
trackpad accumulation, coalescing and serialized channel edits.
