// The board owns acquisition and learning state. This page only presents it.
class AnomalyApp {
  private client: Client;
  private id: number;
  private cmds: Commands;
  private connected = false;
  private busy = false;
  private state: number[] = [];
  private diagnostics: Uint32Array = new Uint32Array(0);
  private trace: Uint32Array = new Uint32Array(0);
  private eventWaveform: Uint32Array = new Uint32Array(0);
  private eventStart = 0;
  private eventSpan = 30000000;
  private plotIsEvent = false;
  private eventRequest = 0;
  private eventContextLoaded = false;
  private lastDiagnostics = 0;
  private plot = <HTMLCanvasElement>document.getElementById("plot");
  private errorPlot = <HTMLCanvasElement>document.getElementById("error-plot");

  constructor() {
    this.buildPredictorRows();
    this.bind();
    this.connect();
  }
  private el(id: string): HTMLElement {
    return document.getElementById(id);
  }
  private cmd(name: string, ...args: any[]): CmdMessage {
    return Command(this.id, this.cmds[name], ...args);
  }
  private signed(value: number): number {
    return value > 0x7fffffff ? value - 0x100000000 : value;
  }
  private number(value: number): string {
    return value.toLocaleString();
  }
  private banner(value: string, error = false) {
    const el = this.el("banner");
    el.textContent = value;
    el.className = error ? "banner error" : "banner";
    el.hidden = !value;
  }
  private button(id: string, enabled: boolean) {
    (<HTMLButtonElement>this.el(id)).disabled = !enabled;
  }
  private async connect() {
    try {
      this.client = new Client(location.hostname, 3);
      await this.client.init();
      const driver = this.client.getDriver("AnomalyDetector");
      this.id = driver.id;
      this.cmds = driver.getCmds();
      this.connected = true;
      this.el("connection").textContent = "Connected";
      this.el("connection").classList.remove("offline");
      this.banner("");
      await this.tick();
    } catch (error) {
      this.connected = false;
      this.el("connection").textContent = "Reconnecting";
      this.el("connection").classList.add("offline");
      this.banner(
        "Board connection interrupted. Reconnecting automatically…",
        true,
      );
      setTimeout(() => this.connect(), 2000);
    }
  }
  private async tick() {
    if (!this.connected) return;
    try {
      this.state = await this.client.readTuple(
        this.cmd("get_state"),
        "IIIIIIIIIIIII",
      );
      const now = Date.now();
      if (now - this.lastDiagnostics > 800) {
        this.diagnostics = await this.client.readUint32Vector(
          this.cmd("get_diagnostics"),
        );
        this.lastDiagnostics = now;
      }
      this.renderState();
      this.renderDiagnostics();
      if (this.state[0] === 5) {
        if (!this.plotIsEvent) {
          this.plotIsEvent = true;
          await this.loadEvent();
        }
      } else if (this.state[0] !== 6 && this.state[0] !== 8) {
        this.plotIsEvent = false;
        this.trace = await this.client.readUint32Vector(
          this.cmd("get_live_trace"),
        );
        this.renderNetwork();
        this.draw();
      }
      setTimeout(() => this.tick(), 160);
    } catch (error) {
      this.connected = false;
      this.el("connection").textContent = "Reconnecting";
      this.el("connection").classList.add("offline");
      this.banner(
        "Board connection interrupted. Reconnecting automatically…",
        true,
      );
      setTimeout(() => this.connect(), 2000);
    }
  }
  private renderState() {
    const s = this.state,
      phase = s[0],
      learned = s[9] !== 0;
    const names = [
      "Collecting ADC history",
      "Model ready",
      "Preparing capture",
      "Detecting continuously",
      "Recording after trigger",
      "Capture held",
      "Acquisition error",
      "Learning from ADC0",
      "Verifying capture",
    ];
    this.el("status").textContent = names[phase] || "Board status";
    this.el("score").textContent = learned ? this.number(s[3]) : "—";
    if (document.activeElement !== this.el("threshold"))
      (<HTMLInputElement>this.el("threshold")).value = String(s[4]);
    let detail =
      "DAC0 is looped to ADC0. Start adaptive learning to fit measured data.";
    if (phase === 7)
      detail = "Learning in the background. The live trace keeps updating.";
    if (phase === 1)
      detail = "Prediction is live. Adjust the threshold or prepare a capture.";
    if (phase === 2)
      detail =
        "Building one second of history: " +
        Math.min(100, Math.floor((s[2] * 131072) / 150000)) +
        "%.";
    if (phase === 3)
      detail = "Capture is armed. Normal data continues to refine the model.";
    if (phase === 4)
      detail = "Recording after trigger: " + (s[8] / 10).toFixed(0) + "%.";
    if (phase === 5)
      detail =
        "Verified 30 million samples. Select a capture window or click the plot to zoom.";
    if (phase === 8)
      detail =
        "Checking every captured sample for gaps: " +
        (s[8] / 10).toFixed(0) +
        "%.";
    if (phase === 6)
      detail =
        "Acquisition fault " +
        s[1] +
        ". Restart the instrument after fixing the cause.";
    this.el("detail").textContent = detail;
    const learning = this.diagnostics.length > 0 && this.diagnostics[0] !== 0;
    this.button(
      "learn",
      !this.busy &&
        !learning &&
        phase !== 4 &&
        phase !== 5 &&
        phase !== 6 &&
        phase !== 8,
    );
    this.button("pause", !this.busy && learning && phase !== 6);
    this.button("arm", !this.busy && phase === 1);
    this.button("inject", !this.busy && phase === 3);
    this.button("again", !this.busy && phase === 5);
    this.button("apply", !this.busy && phase !== 6);
    this.el("zoom-controls").hidden = phase !== 5;
    this.el("error-card").hidden = phase === 5;
    this.el("predicted-legend").hidden = phase === 5;
    this.el("model-view-label").textContent =
      phase === 5 ? "at trigger" : "updated live";
  }
  private renderDiagnostics() {
    const d = this.diagnostics;
    if (d.length < 26) return;
    const learning = d[0] !== 0;
    const held = this.state[0] === 5 || this.state[0] === 8;
    this.el("learning-indicator").textContent = learning
      ? held
        ? "Waiting"
        : "Adapting"
      : "Paused";
    this.el("learning-indicator").classList.toggle("active", learning && !held);
    let learningDetail = "Start learning when ready.";
    if (d[25] === 5)
      learningDetail =
        "ADC0 amplitude is too small. Check the DAC0 → ADC0 cable.";
    else if (d[25])
      learningDetail =
        "Training sample integrity check failed. Waiting for fresh data.";
    else if (held)
      learningDetail =
        this.number(d[1]) +
        (learning
          ? " updates · replace capture to resume learning."
          : " updates · replace capture, then start learning.");
    else if (learning && d[1])
      learningDetail =
        "Updated " + this.number(d[1]) + " times while acquisition continues.";
    else if (learning)
      learningDetail = "Collecting the first second of normal signal…";
    else if (d[1])
      learningDetail = "Learning paused. Detection remains active.";
    this.el("learning-detail").textContent = learningDetail;
    this.el("updates").textContent = this.number(d[1]);
    this.el("rmse").textContent = d[1] ? this.number(d[5]) : "—";
    this.el("p999").textContent = d[1] ? this.number(d[6]) : "—";
    this.el("suggested").textContent = d[1] ? this.number(d[7]) : "—";
    this.el("bias").textContent = d[1]
      ? this.signed(d[8]).toLocaleString()
      : "—";
    this.el("training-points").textContent = d[1] ? this.number(d[2]) : "—";
    this.el("train-mae").textContent = d[1] ? this.number(d[3]) : "—";
    this.el("holdout-mae").textContent = d[1] ? this.number(d[4]) : "—";
    let maximum = 1;
    for (let i = 0; i < 8; i++) maximum = Math.max(maximum, d[17 + i]);
    for (let i = 0; i < 8; i++) {
      this.el("weight-" + i).textContent = d[1]
        ? (this.signed(d[9 + i]) / 4096).toFixed(3)
        : "—";
      this.el("contribution-" + i).textContent = d[1]
        ? this.number(d[17 + i])
        : "—";
      (<HTMLElement>this.el("bar-" + i)).style.width = d[1]
        ? ((100 * d[17 + i]) / maximum).toFixed(1) + "%"
        : "0%";
    }
  }
  private renderNetwork() {
    if (this.trace.length < 3013) return;
    const history: number[] = [];
    for (let lag = 0; lag < 4; lag++) {
      const start = 3000 + 3 * lag;
      history.push(this.signed(this.trace[start]));
    }
    this.showNetwork(
      history,
      this.state[9] ? this.signed(this.trace[3012]) : null,
    );
    this.el("network-error").textContent = "|ADC − x̂| → alert";
  }
  private showNetwork(history: number[], prediction: number | null) {
    for (let lag = 0; lag < 4; lag++) {
      const q = history[lag] >> 2;
      const positive = Math.max(0, q);
      const negative = Math.max(0, -q);
      this.el("input-" + lag).textContent = this.number(history[lag]);
      this.el("positive-" + lag).textContent = this.number(positive);
      this.el("negative-" + lag).textContent = this.number(negative);
      this.el("feature-" + lag).classList.toggle(
        "active",
        positive + negative > 0,
      );
    }
    this.el("network-output").textContent =
      prediction === null ? "—" : this.number(prediction);
  }
  private signedAdc(word: number): number {
    const value = word & 0x3ffff;
    return value & 0x20000 ? value - 262144 : value;
  }
  private async loadTriggerNetwork(request: number) {
    if (this.eventContextLoaded) return;
    const raw = await this.client.readUint32Vector(
      this.cmd("get_event_raw", 14999996, 5),
    );
    if (
      request !== this.eventRequest ||
      raw.length !== 5 ||
      this.diagnostics.length < 26
    )
      return;
    const history = [3, 2, 1, 0].map((i) => this.signedAdc(raw[i]));
    const d = this.diagnostics;
    let sum = 0;
    for (let lag = 0; lag < 4; lag++) {
      const q = history[lag] >> 2;
      sum += Math.max(0, q) * this.signed(d[9 + 2 * lag]);
      sum += Math.max(0, -q) * this.signed(d[10 + 2 * lag]);
    }
    const prediction = Math.max(
      -131072,
      Math.min(131071, (Math.floor(sum / 4096) + this.signed(d[8])) * 4),
    );
    this.showNetwork(history, prediction);
    this.el("network-error").textContent =
      "Trigger error " +
      this.number(Math.abs(this.signedAdc(raw[4]) - prediction)) +
      " counts";
    this.eventContextLoaded = true;
  }
  private buildPredictorRows() {
    const host = this.el("predictors");
    for (let i = 0; i < 8; i++) {
      const row = document.createElement("div");
      row.className = "predictor-row" + (i % 2 ? " negative" : "");
      row.innerHTML =
        "<span>x[n−" +
        (Math.floor(i / 2) + 1) +
        "] " +
        (i % 2 ? "negative" : "positive") +
        "</span>" +
        '<span id="weight-' +
        i +
        '">—</span><span class="contribution"><span id="contribution-' +
        i +
        '">—</span>' +
        '<span class="bar-track"><span id="bar-' +
        i +
        '" class="bar-fill"></span></span></span>';
      host.appendChild(row);
    }
  }
  private async act(name: string, ...args: number[]) {
    if (this.busy || !this.connected) return;
    this.busy = true;
    this.banner("Applying " + name.replace("_", " ") + "…");
    try {
      const result = await this.client.readUint32(this.cmd(name, ...args));
      if (result !== 0) throw new Error(this.actionError(name, result));
      if (name === "try_again") {
        this.plotIsEvent = false;
        this.eventContextLoaded = false;
        this.eventStart = 0;
        this.eventSpan = 30000000;
        this.selectZoom();
      }
      this.lastDiagnostics = 0;
      this.banner("");
    } catch (error) {
      this.banner(String(error), true);
    } finally {
      this.busy = false;
      this.renderState();
    }
  }
  private actionError(name: string, code: number): string {
    if (name === "learn" && code === 4)
      return "Replace the held capture before starting learning.";
    if (name === "set_threshold")
      return "Threshold must be 8–131071 ADC counts.";
    return "Action unavailable (" + name + ", code " + code + ").";
  }
  private canvasContext(canvas: HTMLCanvasElement): CanvasRenderingContext2D {
    const ratio = window.devicePixelRatio || 1;
    canvas.width = Math.max(1, Math.round(canvas.clientWidth * ratio));
    canvas.height = Math.max(1, Math.round(canvas.clientHeight * ratio));
    const ctx = canvas.getContext("2d");
    ctx.scale(ratio, ratio);
    return ctx;
  }
  private grid(ctx: CanvasRenderingContext2D, width: number, height: number) {
    ctx.clearRect(0, 0, width, height);
    ctx.strokeStyle = "#e7eff3";
    ctx.lineWidth = 1;
    for (let i = 1; i < 4; i++) {
      const y = (i * height) / 4;
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(width, y);
      ctx.stroke();
    }
  }
  private line(
    ctx: CanvasRenderingContext2D,
    values: Uint32Array,
    offset: number,
    count: number,
    low: number,
    high: number,
    color: string,
    width: number,
    height: number,
  ) {
    ctx.strokeStyle = color;
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    for (let i = 0; i < count; i++) {
      const x = (i * width) / (count - 1);
      const y = height - ((values[offset + i] - low) * height) / (high - low);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();
  }
  private draw() {
    const ctx = this.canvasContext(this.plot);
    const width = this.plot.clientWidth,
      height = this.plot.clientHeight;
    this.grid(ctx, width, height);
    if (this.plotIsEvent) this.drawEvent(ctx, width, height);
    else this.drawLive(ctx, width, height);
  }
  private drawLive(
    ctx: CanvasRenderingContext2D,
    width: number,
    height: number,
  ) {
    this.el("plot-title").textContent = "Live ADC0 and predictor";
    this.el("left-time").textContent = "0 ms";
    this.el("right-time").textContent = "17.5 ms";
    const values = this.trace;
    if (values.length < 3000) return;
    let low = 262143,
      high = 0;
    for (let i = 0; i < 2000; i++) {
      low = Math.min(low, values[i]);
      high = Math.max(high, values[i]);
    }
    const pad = Math.max(100, (high - low) * 0.12);
    low -= pad;
    high += pad;
    this.line(ctx, values, 0, 1000, low, high, "#019cd5", width, height);
    ctx.setLineDash([7, 5]);
    this.line(ctx, values, 1000, 1000, low, high, "#dc8734", width, height);
    ctx.setLineDash([]);
    const error = this.canvasContext(this.errorPlot);
    const ew = this.errorPlot.clientWidth,
      eh = this.errorPlot.clientHeight;
    this.grid(error, ew, eh);
    const threshold = this.state[4] || 3000;
    let maxError = threshold * 1.3;
    for (let i = 2000; i < 3000; i++)
      maxError = Math.max(maxError, values[i] * 1.15);
    const y = eh - (threshold * eh) / maxError;
    error.strokeStyle = "#d8896b";
    error.setLineDash([5, 4]);
    error.beginPath();
    error.moveTo(0, y);
    error.lineTo(ew, y);
    error.stroke();
    error.setLineDash([]);
    this.line(error, values, 2000, 1000, 0, maxError, "#019cd5", ew, eh);
    this.el("error-scale").textContent = "Threshold " + this.number(threshold);
  }
  private drawEvent(
    ctx: CanvasRenderingContext2D,
    width: number,
    height: number,
  ) {
    this.el("plot-title").textContent =
      "Captured ADC0 · 1 s before / 1 s after";
    this.el("left-time").textContent = this.timeLabel(
      this.eventStart - 15000000,
    );
    this.el("right-time").textContent = this.timeLabel(
      this.eventStart + this.eventSpan - 15000000,
    );
    const values = this.eventWaveform;
    if (values.length < 2000) return;
    let low = 262143,
      high = 0;
    for (let i = 0; i < values.length; i++) {
      low = Math.min(low, values[i]);
      high = Math.max(high, values[i]);
    }
    const pad = Math.max(100, (high - low) * 0.12);
    low -= pad;
    high += pad;
    ctx.strokeStyle = "#019cd5";
    ctx.lineWidth = 1.1;
    for (let i = 0; i < 1000; i++) {
      const x = ((i + 0.5) * width) / 1000;
      const y0 = height - ((values[2 * i] - low) * height) / (high - low);
      const y1 = height - ((values[2 * i + 1] - low) * height) / (high - low);
      ctx.beginPath();
      ctx.moveTo(x, y0);
      ctx.lineTo(x, y1);
      ctx.stroke();
    }
    if (this.eventSpan <= 15000) {
      ctx.strokeStyle = "#027ea9";
      ctx.lineWidth = 1.7;
      ctx.beginPath();
      for (let i = 0; i < 1000; i++) {
        const x = ((i + 0.5) * width) / 1000;
        const mid = (values[2 * i] + values[2 * i + 1]) / 2;
        const y = height - ((mid - low) * height) / (high - low);
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();
    }
    const trigger = (15000000 - this.eventStart) / this.eventSpan;
    if (trigger >= 0 && trigger <= 1) {
      ctx.strokeStyle = "#ec795e";
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.moveTo(trigger * width, 0);
      ctx.lineTo(trigger * width, height);
      ctx.stroke();
    }
  }
  private timeLabel(sample: number): string {
    const seconds = sample / 15000000;
    if (this.eventSpan <= 15000) return (seconds * 1000000).toFixed(0) + " µs";
    if (this.eventSpan <= 1500000) return (seconds * 1000).toFixed(1) + " ms";
    return seconds.toFixed(3) + " s";
  }
  private async loadEvent() {
    const request = ++this.eventRequest;
    const waveform = await this.client.readUint32Vector(
      this.cmd("get_event", this.eventStart, this.eventSpan),
    );
    if (request !== this.eventRequest || !this.plotIsEvent) return;
    this.eventWaveform = waveform;
    this.draw();
    await this.loadTriggerNetwork(request);
  }
  private selectZoom() {
    for (const button of <HTMLButtonElement[]>(
      (<any>document.querySelectorAll("[data-span]"))
    ))
      button.classList.toggle(
        "selected",
        Number(button.dataset.span) === this.eventSpan,
      );
  }
  private bind() {
    this.el("learn").onclick = () => this.act("learn");
    this.el("pause").onclick = () => this.act("set_learning", 0);
    this.el("arm").onclick = () => this.act("arm");
    this.el("inject").onclick = () => this.act("inject");
    this.el("again").onclick = () => this.act("try_again");
    this.el("apply").onclick = () =>
      this.act(
        "set_threshold",
        Number((<HTMLInputElement>this.el("threshold")).value),
      );
    this.plot.onclick = (ev) => {
      if (!this.plotIsEvent) return;
      const ratio =
        (ev.clientX - this.plot.getBoundingClientRect().left) /
        this.plot.clientWidth;
      const center = this.eventStart + ratio * this.eventSpan;
      if (this.eventSpan === 30000000) this.eventSpan = 1500000;
      this.eventStart = Math.max(
        0,
        Math.min(
          30000000 - this.eventSpan,
          Math.round(center - this.eventSpan / 2),
        ),
      );
      this.selectZoom();
      this.loadEvent();
    };
    for (const button of <HTMLButtonElement[]>(
      (<any>document.querySelectorAll("[data-span]"))
    )) {
      button.onclick = () => {
        const trigger = 15000000;
        const center =
          this.eventStart <= trigger &&
          trigger <= this.eventStart + this.eventSpan
            ? trigger
            : this.eventStart + this.eventSpan / 2;
        this.eventSpan = Number(button.dataset.span);
        this.eventStart = Math.max(
          0,
          Math.min(
            30000000 - this.eventSpan,
            Math.round(center - this.eventSpan / 2),
          ),
        );
        this.selectZoom();
        this.loadEvent();
      };
    }
    window.onresize = () => this.draw();
    window.onbeforeunload = () => {
      if (this.client) this.client.exit();
    };
  }
}
new AnomalyApp();
