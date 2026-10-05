interface PnaSpectrumFrame<P> {
  sequence: number;
  state: number;
  precision: number;
  parameters: P;
  values: Float32Array;
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
