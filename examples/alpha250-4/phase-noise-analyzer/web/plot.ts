// Board adapter for synchronized X/Y phase differences and signed CSD.
class Plot extends PnaPlot<IParameters> {
  protected signedSpectrum(): boolean { return true; }
  protected localOscillators(p: IParameters): number[] {
    return p.channel === 0 ? [p.fdds0, p.fdds1]
      : p.channel === 1 ? [p.fdds2, p.fdds3]
      : [p.fdds0, p.fdds1, p.fdds2, p.fdds3];
  }
  protected referenceLabel(p: IParameters): string {
    return `${["X", "Y", "XY"][p.channel]} · CIC ${p.cic_rate} · ${p.channel === 2 ? p.avgxy_count + " cumulative segments" : "N " + p.fft_navg}`;
  }
}
