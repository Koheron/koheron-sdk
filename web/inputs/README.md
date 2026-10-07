# Shared digit editors

Include `components.mk` to package `digit-input.ts`. `DigitInput` owns text entry,
selected-digit tuning, validation, asynchronous commit/readback and disposal.
`FrequencyInput` adds frequency units; `NumberInput` supplies other numeric units.
The controls own no transport. Hosts supply limits, resolution and a commit
function returning the accepted value, and dispose editors during teardown.

FFT, PNA, DPLL and the phase-modulator widget use this same implementation.
Its existing CSS class names are retained so each host keeps its styling.
Consumer regressions run through `bash web/tests/run.sh`.
