# Shared instrument styles

Include `components.mk`, load `instrument.css` after the legacy base styles and
before the instrument's own styles, and add `instrument-workspace` to the body.
PNA components include this asset for all three PNAs and DPLL; FFT components
include it for ALPHA15, ALPHA250, ALPHA250-4 and Red Pitaya.

This stylesheet owns common colors, headers, buttons, segmented controls, focus
and validation states, digit editor appearance, and connection status geometry.
Use `digit-control` (or the existing PNA `pna-digit-control`) around digit editors
to opt into floating validation hints; embedded widgets retain their own hints.
Add `instrument-disclosures` to a sidebar for the common disclosure markers.
Hosts keep their grids, breakpoints, control widths and diagram styles locally.
`--instrument-font`, `--control-padding` and `--disclosure-gap` preserve host
variants without copying the base rules. Phase-specific telemetry geometry and
colors remain in `web/phase-noise/phase-precision.css`.

Changes should be checked in both PNA and DPLL layouts, including compact widths,
keyboard focus, disabled controls, and connecting/live/error telemetry states.
Status updates must not move adjacent controls. FFT also uses the shared paused
status color and disclosure markers. Its 26-pixel controls and workspace grid
remain host layout choices through the existing CSS variables. These styles introduce no
JavaScript behavior or hardware commands.

## Slow telemetry lifecycle

Include `poller.mk` for `InstrumentPoller`. Supply a Promise read, a renderer
and optionally an error handler; call `start()` after controls are ready and
`dispose()` on exit. Reads never overlap, skip hidden pages, retry after errors
and run once per second after the previous read finishes. Disposal cancels the
timer and discards pending results. This helper owns no hardware commands.

ALPHA15 groups its board readbacks with this lifecycle. ALPHA250 and ALPHA250-4
FFT use the same poller with their board decoders, independently of the faster
acquisition-control loop. Temperature readouts use one decimal across all three.

## Command listener lifecycle

Include `events.mk` for `InstrumentEvents`. Register a host's DOM commands with
`listen(target, type, handler)` and call `dispose()` on exit. Disposal removes
all registered listeners, guards callbacks already captured by event dispatch
and ignores later registrations. FFT acquisition controls, ALPHA15 input ranges
and PNA channel, tracking, analyzer-mode and cumulative-reset controls share
this lifecycle. Clock selectors, PNA save actions and passive DPLL-monitor
commands also use it; hosts retain their own command mapping and error handling.
`events.mk` can be included through several component lists and packages its
source only once.
