class PhaseModulatorApp {
    private client: Client;
    private widget: PhaseModulatorWidget;
    private stopped = false;

    constructor() {
        this.client = new Client(location.hostname, 2);
        window.addEventListener('pagehide', () => this.dispose());
        void this.init();
    }

    private async init(): Promise<void> {
        const status = document.getElementById('connection-status');
        const root = document.getElementById('phase-modulator');
        root.classList.add('dds-pm-widget');
        root.innerHTML = PhaseModulatorWidget.loadingMarkup(2);
        root.addEventListener('dds-pm-ready', () => {
            status.textContent = 'Connected';
            status.dataset.state = 'live';
        });
        try {
            await this.client.init();
            if (this.stopped) { return; }
            this.widget = new PhaseModulatorWidget(root, new PhaseModulatorDriver(this.client));
            await this.widget.init();
            if (this.stopped) { return; }
            status.textContent = 'Connected';
            status.dataset.state = 'live';
        } catch (error) {
            if (this.stopped) { return; }
            status.textContent = 'Disconnected';
            status.dataset.state = 'error';
            if (!this.widget) {
                const error = document.createElement('div');
                error.className = 'pm-load-error';
                error.setAttribute('role', 'alert');
                const message = document.createElement('span');
                message.textContent = 'Unable to connect to the instrument.';
                error.appendChild(message);
                const retry = document.createElement('button');
                retry.textContent = 'Retry';
                retry.addEventListener('click', () => location.reload());
                error.appendChild(retry);
                root.appendChild(error);
                this.client.exit();
            }
        }
    }

    dispose(): void {
        this.stopped = true;
        if (this.widget) { this.widget.dispose(); }
        this.client.exit();
    }
}

const phaseModulatorApp = new PhaseModulatorApp();
