# Plot references

`components.mk` supplies the shared collection, panel and styles used by FFT,
PNA and DPLL. Add `<div id="plot-references"></div>` to the workspace and load
`plot-references.css` after `instrument.css`. The panel mounts the same controls
in every consumer; `PlotReferencePanel.mount` can create them before plot setup.

`PlotReferences<T>` owns the eight-capture limit, names/colors, removal,
recapture/undo and atomic import append. Adapters copy raw samples and settings,
validate their own file format before appending, and omit rendering caches from
saved files. The panel receives metadata and recapture callbacks. Dispose the
panel with the workspace so a pending file read cannot update a closed view.

FFT regression coverage lives in `web/fft/tests/test_references.cjs`; PNA/DPLL
coverage in `web/phase-noise/tests/test_references.cjs` exercises all four page
layouts, signed data, independent frequency grids and exports.
