// Deterministic transport used by DOM tests and the real-browser UI check.
module.exports = function installMonitorClient(w) {
  const state = w.monitorTest = {writes: [], calls: [], channel: 0, rate: 20, navg: 4, bits: 8,
    sequence: 1, failed: false, closed: 0, frequencies: [10e6, 12e6], gains: [-8, 16, 0, -32, 64, 0, 0, 128]};
  w.MockCommand = (id, name, ...args) => ({id, name, args});
  w.MockClient = class {
    constructor(ip, pool) { if (pool !== 1) throw new Error('Expected ordered socket'); }
    async init() {}
    exit() { state.closed++; }
    getDriver(id) { return {id, getCmds: () => new Proxy({}, {get: (_, key) => key})}; }
    send(command) {
      state.writes.push(command);
      const {id, name, args} = command;
      if (id === 'Dma') {
        if (name === 'set_channel') state.channel = args[0];
        if (name === 'set_cic_rate') state.rate = args[0];
        if (name === 'set_fft_navg') state.navg = args[0];
        state.sequence++;
      }
      if (name === 'set_dds_freq') state.frequencies[args[0]] = args[1];
    }
    check(command) {
      state.calls.push(command);
      if (state.failed) throw new Error('Disconnected test transport');
    }
    async readTuple(command) {
      this.check(command);
      switch(command.name) {
        case 'get_parameters': return [16385, 250e6/(2*state.rate), state.channel, state.rate, state.navg, ...state.frequencies, 0, 0, 2];
        case 'get_average_status': return [state.navg, state.navg];
        case 'get_measurements': return [.01234, 196.35e-12, 1e3, 1e5, -3.21];
        case 'get_precision_status': return [state.bits, state.bits, 6.28e-6, 1, 0, 100, 0, 0, 0, 0, 1, 10];
        case 'get_stream_status': return [0, 100, 0, 0, 32768, 16384, 3];
        case 'get_control_parameters': return [...state.frequencies, ...state.gains, 5, 10];
        case 'get_dac_outputs': return [0, 1];
        default: throw new Error(command.name);
      }
    }
    async readTupleWithFloat32Vector(command) {
      this.check(command);
      const fs = 250e6 / (2 * state.rate);
      const values = new Float32Array(16385);
      for (let i = 2; i < values.length; i++) {
        const f = i * fs / 32768;
        values[i] = 2 * 10 ** ((-133 + 23/(1+(f/2500)**2) + 30*Math.exp(-(((f-10000)/500)**2)) + Math.sin(i*1.7)*3) / 10);
      }
      return {metadata: [state.sequence, 1, state.bits, fs, state.channel, state.rate, state.navg,
        state.navg, state.navg, ...state.frequencies, 0, 0, 0, 0, 2], values};
    }
    async readBool(command) {
      this.check(command); this.send(command);
      state.bits = command.args[0]; return true;
    }
    async readUint32(command) { this.check(command); return 2; }
    async readFloat64(command) { this.check(command); return 250e6; }
    async readFloat64Array(command) { this.check(command); return new Float64Array(state.gains); }
    async readInt32(command) { this.check(command); this.send(command); return 0; }
  };
  return state;
};
