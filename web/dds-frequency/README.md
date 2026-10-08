# DDS frequency controls

Include `components.mk` for the shared frequency digit editor, its template and
styles, and telemetry poller. Red Pitaya dual-DDS supplies frequency writes and
Promise readbacks in Hz. Startup reads settings; each output supports 0–125 MHz
with 1 Hz entry resolution, retaining the instrument's existing frequency range.
Typed values commit on Enter/change; arrows and wheel tune the selected digit.
Readbacks preserve drafts. Call `dispose()` on exit.

The component contains no sliders. Pulse generator does not use DDS controls.
`tests/test_controls.cjs` exercises the real dual-DDS page and adapter through
the [shared browser runner](../tests/README.md).
