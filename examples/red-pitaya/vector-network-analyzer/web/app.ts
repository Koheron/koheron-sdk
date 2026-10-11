const VNAProfile: Readonly<VNAInstrumentProfile> = Object.freeze({
    board: 'red-pitaya', label: 'Red Pitaya', sampleRate: 125e6, maxFrequency: 50e6, sourceShift: 2,
    output: 'OUT1', input: 'IN1', mutedOutput: 'OUT2'
});
mountVNA();
