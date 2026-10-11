interface VNAInstrumentProfile {
    board: string; label: string; sampleRate: number; maxFrequency: number; sourceShift: number;
    output: string; input: string; mutedOutput: string;
}
// Both boards build the same workspace and worker. Only their physical limits
// and connector labels differ; the board entry point selects these before use.
// Capture geometry is a protocol contract shared by the workspace and worker.
const VNAProtocol = Object.freeze({
    captureSamples: 64 * 1024 * 1024,
    packetBytes: 256 * 1024,
    batchBlocks: 16,
    planWords: 36
});
function mountVNA(): void {
    Object.assign(globalThis, {VNA: {Client, VNADriver, VNAModel, VNAPlot, VNAWorkspace, protocol: VNAProtocol}});
    if (typeof document !== 'undefined') {
        Object.assign(globalThis, {vna: new VNAWorkspace(window, document,
            new URLSearchParams(location.search).get('demo') === '1')});
    }
}
