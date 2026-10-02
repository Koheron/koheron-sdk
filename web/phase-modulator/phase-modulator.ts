// Shared transport adapter for a phase-modulator widget. The host owns Client.
interface PhaseModulatorInfo {
    sampleRate: number;
    phaseWidth: number;
    prbsWidth: number;
    capabilities: number;
}

interface PhaseModulatorSettings {
    carrier: number; // Hz
    phase: number; // degrees
    modulation: number; // Hz
    deviation: number; // degrees
    duty: number; // fraction
    seed: number;
    waveform: number;
    output: boolean;
    pm: boolean;
}

type PhaseModulatorField = keyof PhaseModulatorSettings;

interface PhaseModulatorPort {
    init(): Promise<PhaseModulatorInfo[]>;
    settings(channel: number): Promise<PhaseModulatorSettings>;
    set(channel: number, field: PhaseModulatorField, value: number | boolean): Promise<void>;
    restart(channel: number): Promise<void>;
}

class PhaseModulatorDriver implements PhaseModulatorPort {
    private id: number;
    private commands: Commands;
    private info: PhaseModulatorInfo[] = [];
    private initialization: Promise<PhaseModulatorInfo[]>;

    constructor(private client: Client, driverName: string = 'PhaseModulator') {
        const driver = client.getDriver(driverName);
        this.id = driver.id;
        this.commands = driver.getCmds();
    }

    private command(name: string, ...args: any[]): CmdMessage {
        if (!this.commands[name]) { throw new Error('Update the instrument server to use the phase-modulator widget.'); }
        return Command(this.id, this.commands[name], ...args);
    }

    init(): Promise<PhaseModulatorInfo[]> {
        if (!this.initialization) {
            this.initialization = this.discover().catch(error => {
                this.initialization = undefined;
                throw error;
            });
        }
        return this.initialization;
    }

    private async discover(): Promise<PhaseModulatorInfo[]> {
        const count = await this.client.readUint32(this.command('get_channel_count'));
        if (count !== 1 && count !== 2) {
            throw new Error(await this.client.readString(this.command('get_initialization_error')) || 'No usable DDS channels.');
        }
        const sampleRate = await this.client.readUint32(this.command('get_sample_rate'));
        if (!(sampleRate > 0)) { throw new Error('Invalid sampling rate.'); }
        this.info = [];
        for (let channel = 0; channel < count; channel++) {
            const values = await this.client.readTuple(this.command('get_channel_info', channel), 'IIIIII');
            if (values[0] < 32 || values[0] > 48) { throw new Error('Unsupported DDS phase width.'); }
            this.info.push({sampleRate, phaseWidth: values[0], prbsWidth: values[3], capabilities: values[5]});
        }
        return this.info;
    }

    async settings(channel: number): Promise<PhaseModulatorSettings> {
        if (!this.info[channel]) { throw new Error('DDS channel is absent.'); }
        // Decode each uint64 as two network-order uint32s. All words are <= 2^48,
        // so Number represents them exactly without BigInt or shared-client changes.
        const words = await this.client.readTuple(this.command('get_settings_words', channel), 'IIIIIIIIIIIIIII??');
        if (words[0]) { throw new Error(await this.client.readString(this.command('get_error_message', words[0]))); }
        const word = (index: number) => words[1 + 2 * index] * 4294967296 + words[2 + 2 * index];
        const turn = Math.pow(2, this.info[channel].phaseWidth);
        return {carrier: word(0) / turn * this.info[channel].sampleRate,
                phase: word(1) / turn * 360, modulation: word(2) / turn * this.info[channel].sampleRate,
                deviation: word(4) / turn * 360, duty: word(5) / turn,
                seed: words[13], waveform: words[14], output: words[15], pm: words[16]};
    }

    private async checked(name: string, ...args: any[]): Promise<void> {
        const message = await this.client.readString(this.command(name, ...args));
        if (message) { throw new Error(message); }
    }

    set(channel: number, field: PhaseModulatorField, value: number | boolean): Promise<void> {
        const setters: {[field: string]: string} = {
            carrier: 'set_carrier_frequency', phase: 'set_phase', modulation: 'set_modulation_frequency',
            deviation: 'set_deviation', duty: 'set_duty', seed: 'set_seed', waveform: 'set_waveform',
            output: 'set_output_enabled', pm: 'set_pm_enabled'
        };
        return this.checked(setters[field], channel, value);
    }

    restart(channel: number): Promise<void> { return this.checked('restart', channel); }
}
