let app = new FFTWorkspace(window, document, location.hostname, {
    boardName: 'Red Pitaya',
    halfScaleOutput: true,
    createDriver: client => new FFT(client),
    createBoard: (_client, document) => new RedPitayaFFTControls(document)
});
