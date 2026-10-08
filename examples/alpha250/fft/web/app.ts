let app = new FFTWorkspace(window, document, location.hostname, {
    boardName: 'ALPHA250',
    createDriver: client => new FFT(client),
    createBoard: (client, document) => new AlphaFFTControls(client, document)
});
