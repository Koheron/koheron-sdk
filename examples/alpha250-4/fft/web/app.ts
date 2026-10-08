let app = new FFTWorkspace(window, document, location.hostname, {
    boardName: 'ALPHA250-4',
    signalGenerator: false,
    createDriver: client => new FFT(client),
    createBoard: (client, document, driver) => new AlphaFFTControls(client, document,
        () => (driver as FFT).settingsChanged())
});
