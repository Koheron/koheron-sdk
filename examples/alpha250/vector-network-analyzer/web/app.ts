const VNAProfile: Readonly<VNAInstrumentProfile> = Object.freeze({
    board: 'alpha250', label: 'ALPHA250', sampleRate: 250e6, maxFrequency: 100e6, sourceShift: 0,
    output: 'OUT0', input: 'IN0', mutedOutput: 'OUT1'
});
mountVNA();
