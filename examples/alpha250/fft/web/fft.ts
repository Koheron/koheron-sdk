// Board-specific FFT response decoder; transport and streaming are shared.
type TupleGetParameters = [number, number, number, number, number, number, number, number];

class FFT extends FFTDriver {
    async getControlParameters(): Promise<IFFTStatus> {
        const [fdds0, fdds1, fs, channel, W1, W2, window_index, clkin] =
        await this.client.readTuple<TupleGetParameters>(
            Command(this.id, this.cmds['get_control_parameters']),
            'dddIddII'
        );

        let clkIndex: string = "0";

        if (clkin !== 0) {
            clkIndex = "2";
        }

        this.status = {dds_freq: [fdds0, fdds1], fs, channel, W1, W2, window_index, clkIndex};
        return this.status;
    }

    async getBoardParameters(): Promise<IBoardParameters> {
        const arr = await this.client.readFloat32Array(
            Command(this.id, this.cmds['get_board_parameters']));

        const supplyValues = arr.slice(0, 4);
        const adcValues    = arr.slice(4, 12);
        const dacValues    = arr.slice(12, 16);
        const temperatures = arr.slice(16, 19);

        return {supplyValues, adcValues, dacValues, temperatures};
    }
}
