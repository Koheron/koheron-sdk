// Board adapter for the shared PNA spectrum display.
class Plot extends PnaPlot<IParameters> {
  protected signedSpectrum(): boolean { return false; }
  protected localOscillators(p: IParameters): number[] {
    return [p.channel === 0 ? p.fdds0 : p.fdds1];
  }
  protected referenceLabel(p: IParameters): string {
    return `ADC ${p.channel} · CIC ${p.cic_rate} · N ${p.fft_navg} · ${p.analyzer_mode.toUpperCase()}`;
  }
}
