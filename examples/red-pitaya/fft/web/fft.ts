// Board-specific FFT response decoder; transport and streaming are shared.
type TupleGetParameters = [number, number, number, number, number, number];

class FFT extends FFTDriver {
    async getControlParameters(): Promise<IFFTStatus> {
        const [fdds0, fdds1, fs, channel, W1, W2] =
            await this.client.readTuple<TupleGetParameters>(
                Command(this.id, this.cmds['get_control_parameters']), 'dddIdd');
        const window_index = await this.client.readUint32(
            Command(this.id, this.cmds['get_window_index']));
        this.status = {dds_freq: [fdds0, fdds1], fs, channel, W1, W2,
                       window_index, clkIndex: 'fixed'};
        return this.status;
    }
}
