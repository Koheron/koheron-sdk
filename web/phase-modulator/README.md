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
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator-widget.ts
WEB_FILES += $(SDK_PATH)/web/phase-modulator/phase-modulator.css
```

Load `phase-modulator.css` in the host page and provide a mount element:

```html
<link rel="stylesheet" href="phase-modulator.css">
<div id="generator"></div>
```

After the host application's shared Koheron `Client` is initialized:

```typescript
const driver = new PhaseModulatorDriver(client);
const generator = new PhaseModulatorWidget(document.getElementById('generator'), driver);
await generator.init();

// During host teardown:
generator.dispose();
```

The widget owns neither the socket pool nor the page lifecycle. The host calls
`client.exit()` when its application closes. All DOM access stays inside the
mount element, CSS is scoped to `.dds-pm-widget`, and there are no generated
global IDs, imports, polling loops or page-level event listeners. Multiple
widgets can share a client and transport adapter. The host can listen for the
bubbling `dds-pm-ready` event to update its connection indicator.

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

Every edit uses a checked server setter that reads/modifies/commits under the
channel lock. Other settings, including native phase words, are preserved.
Oscillators keep running through edits and mute; **Restart phase** is explicit.
Only the channel being updated is locked while awaiting acknowledgement.
The widget reads accepted settings after each operation and displays failures
inline. Refresh never retries a failed write. Closing the widget removes its
listeners and ignores late responses; an already-issued command can still
complete in hardware.

The adapter decodes 48-bit native readback exactly as pairs of uint32 words.
The UI displays Hz-derived MHz/kHz, degrees and percent duty; the shortest
displayed number within half a hardware LSB hides quantization artifacts such
as `9.9999999996 kHz`. Hover over a numeric field for its accepted value.
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
timeouts, retry, disposal and 48-bit native readback.
