// dB describes the coefficient magnitude, relative to |g| = 1; polarity is separate.
class DpllGain {
  static readonly dbPerOctave = 20 * Math.log(2) / Math.LN10;
  // Match gain_control.hpp so an arrow step previews the exact programmed coefficient.
  private static readonly mantissas = [2048,2139,2233,2332,2435,2543,2656,2774,2896,3025,3158,3298,3444,3597,3756,3922];

  static db(gain: number): string { return (20 * Math.log(Math.abs(gain)) / Math.LN10).toFixed(2); }
  static step(gain: number): number { return Math.round(Math.log(Math.abs(gain)) / Math.LN2 * 16); }
  static coefficient(step: number): number {
    return this.mantissas[step % 16] / 2048 * Math.pow(2, Math.floor(step / 16));
  }
  static value(step: number, unit: string): string {
    return unit === 'db' ? this.db(this.coefficient(step)) : String(step / 16);
  }
  static inputStep(value: string, unit: string): number {
    return Math.round(Number(value) * 16 / (unit === 'db' ? this.dbPerOctave : 1));
  }
  static description(gain: number): string {
    return gain === 0 ? 'Off · coefficient 0' : `${gain < 0 ? 'Negative' : 'Positive'} · ${this.db(gain)} dB · coefficient ${gain}`;
  }
}
