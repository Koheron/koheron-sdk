let app = new FFTWorkspace(window, document, location.hostname, {
    boardName: 'ALPHA15', instrumentName: 'Signal analyzer', signalGenerator: false,
    createDriver: client => new FFT(client),
    createBoard: (client, document) => new Alpha15SignalAnalyzerControls(client, document)
});
