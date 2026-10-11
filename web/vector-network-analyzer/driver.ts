class VNADriver {
    private id: number;
    private commands: Commands;
    constructor(private client: Client) {
        const driver = client.getDriver('VectorNetworkAnalyzer');
        this.id = driver.id; this.commands = driver.getCmds();
    }
    private command(name: string, ...args: number[]): CmdMessage {
        if (!this.commands[name]) throw new Error('Instrument protocol is incompatible. Install the VNA instrument.');
        return Command(this.id, this.commands[name], ...args);
    }
    async begin(settings: VNASettings): Promise<number> {
        const [token, error] = await this.client.readTuple(this.command('begin_recorded_sweep',
            settings.start / 2, settings.stop * 1.1, settings.duration, settings.level, VNAModel.recordSamples(settings)), 'II');
        if (!token || error) throw new Error([
            'Acquisition could not start.', 'Sweep settings are outside the instrument limits.',
            'Another browser owns the acquisition. Finish its sweep or wait 30 seconds.',
            'DMA did not reset. Restart the instrument.'
        ][error] || 'Acquisition could not start.');
        return token;
    }
    plan(token: number): Promise<Uint32Array> {
        return this.client.readUint32Array(this.command('get_plan', token));
    }
    status(token: number): Promise<number> {
        return this.client.readUint32(this.command('get_status', token));
    }
    recordSamples(token: number): Promise<number> {
        return this.client.readUint32(this.command('get_record_samples', token));
    }
    blocks(token: number, index: number, count: number): Promise<Uint32Array> {
        return this.client.readUint32Vector(this.command('get_adc_blocks', token, index, count));
    }
    release(token: number): Promise<boolean> {
        return this.client.readBool(this.command('release', token));
    }
}
