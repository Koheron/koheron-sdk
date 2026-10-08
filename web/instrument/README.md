# Shared instrument styles

Include `components.mk`, load `instrument.css` after the legacy base styles and
before the instrument's own styles, and add `instrument-workspace` to the body.
PNA components already include this asset for all three PNAs and DPLL.

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
Status updates must not move adjacent controls. These styles introduce no
JavaScript behavior or hardware commands.
