interface PnaSpectrumFrame<P> {
  sequence: number;
  state: number;
  precision: number;
  parameters: P;
  values: Float32Array;
  transferredDensityBytes?: number;
}

interface PnaSpectrumMetadata {
  sequence: number; state: number; precision: number;
  data_size: number; fs: number; channel: number; cic_rate: number;
  fft_navg: number; avgxy_count: number; average_target: number;
  fdds0: number; fdds1: number; fdds2: number; fdds3: number;
  analyzer_mode: string; interferometer_delay: number; clkIndex: string;
}

async function readPnaSpectrum(client: Client, cmd: CmdMessage): Promise<{metadata: PnaSpectrumMetadata; values: Float32Array}> {
  const reply = await client.readTupleWithFloat32Vector(cmd, 'QIIdIIIIIddddIdI', 92);
  const [sequence, state, precision, fs, channel, cic_rate, fft_navg, avgxy_count, average_target,
    fdds0, fdds1, fdds2, fdds3, mode, interferometer_delay, clock] = reply.metadata;
  return {values: reply.values, metadata: {sequence, state, precision,
    data_size: reply.values.length, fs, channel, cic_rate, fft_navg, avgxy_count, average_target,
    fdds0, fdds1, fdds2, fdds3, analyzer_mode: mode === 0 ? 'rf' : 'laser',
    interferometer_delay, clkIndex: String(clock)}};
}

// Each browser connection owns its last full vector. A periodic full read also
// bounds stale data if a board restarts and reuses the same publication number.
class PnaSpectrumReader {
  private cached: {sequence: number; values: Float32Array};
  private lastFullAt = -Infinity;

  constructor(private client: Client, private id: number, private commands: Commands) {}

  async read(): Promise<{metadata: PnaSpectrumMetadata; values: Float32Array; transferredDensityBytes: number}> {
    const since = this.commands['get_spectrum_snapshot_since'];
    const conditional = !!since && !!this.cached && performance.now() - this.lastFullAt < 5000;
    const sequence = this.cached?.sequence ?? 0;
    const command = conditional
      ? Command(this.id, since, Math.floor(sequence / 0x100000000), sequence >>> 0)
      : Command(this.id, this.commands['get_spectrum_snapshot']);
    try {
      const reply = await readPnaSpectrum(this.client, command);
      const transferredDensityBytes = reply.values.byteLength;
      if (conditional && !reply.values.length) {
        if (reply.metadata.sequence !== sequence) { throw new Error('Changed spectrum has no density'); }
        return {metadata: {...reply.metadata, data_size: this.cached.values.length},
          values: this.cached.values, transferredDensityBytes};
      }
      if (!reply.values.length) { throw new Error('Empty spectrum density'); }
      this.cached = {sequence: reply.metadata.sequence, values: reply.values};
      this.lastFullAt = performance.now();
      return {...reply, transferredDensityBytes};
    } catch (error) {
      this.cached = undefined;
      throw error;
    }
  }
}
