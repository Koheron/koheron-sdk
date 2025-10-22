interface LogRateSession {
    id: number;
    socket: string;
    rx_total: number;
    tx_total: number;
    rx_mean: number;
    rx_win: number;
    rx_inst: number;
    rx_ewma: number;
    rx_max: number;
    tx_mean: number;
    tx_win: number;
    tx_inst: number;
    tx_ewma: number;
    tx_max: number;
}

interface LogRateResponse {
    ts: number;
    sessions: LogRateSession[];
}

type LogsRateCallback = (payload: LogRateResponse) => void;
type LogsRateErrorCallback = (err: Error) => void;

class LogsRateClient {
    private endpoint: string;
    private pollInterval: number;
    private timer: number | null;
    private onUpdate: LogsRateCallback;
    private onError?: LogsRateErrorCallback;

    constructor(endpoint: string, pollInterval: number, onUpdate: LogsRateCallback, onError?: LogsRateErrorCallback) {
        this.endpoint = endpoint;
        this.pollInterval = pollInterval;
        this.timer = null;
        this.onUpdate = onUpdate;
        this.onError = onError;
    }

    public start(): void {
        if (this.timer !== null) {
            return;
        }
        this.poll();
    }

    public stop(): void {
        if (this.timer !== null) {
            window.clearTimeout(this.timer);
            this.timer = null;
        }
    }

    private scheduleNext(): void {
        this.timer = window.setTimeout(() => this.poll(), this.pollInterval);
    }

    private async poll(): Promise<void> {
        try {
            const resp = await fetch(this.endpoint, { cache: "no-store" });
            if (!resp.ok) {
                throw new Error("HTTP " + resp.status);
            }
            const data = await resp.json() as LogRateResponse;
            if (!data || !data.sessions || !Array.isArray(data.sessions)) {
                throw new Error("Invalid payload");
            }
            this.onUpdate(data);
        } catch (err) {
            if (this.onError) {
                const error = err instanceof Error ? err : new Error(String(err));
                this.onError(error);
            }
        } finally {
            this.scheduleNext();
        }
    }
}

class LogsRateChart {
    private canvas: HTMLCanvasElement;
    private ctx: CanvasRenderingContext2D;
    private maxPoints: number;
    private rxSeries: number[];
    private txSeries: number[];

    constructor(canvas: HTMLCanvasElement, maxPoints: number) {
        const ctx = canvas.getContext("2d");
        if (!ctx) {
            throw new Error("Cannot initialize chart context");
        }
        this.canvas = canvas;
        this.ctx = ctx;
        this.maxPoints = maxPoints;
        this.rxSeries = [];
        this.txSeries = [];
    }

    public resize(): void {
        const parent = this.canvas.parentElement;
        if (parent) {
            const width = parent.clientWidth;
            if (width > 0) {
                this.canvas.width = width;
            }
            const height = parent.clientHeight;
            if (height > 0) {
                this.canvas.height = height;
            }
        }

        if (this.canvas.width === 0) {
            this.canvas.width = 600;
        }
        if (this.canvas.height === 0) {
            this.canvas.height = 240;
        }

        this.render();
    }

    public addSample(rxInst: number, txInst: number): void {
        this.rxSeries.push(rxInst);
        this.txSeries.push(txInst);

        if (this.rxSeries.length > this.maxPoints) {
            this.rxSeries.shift();
        }
        if (this.txSeries.length > this.maxPoints) {
            this.txSeries.shift();
        }

        this.render();
    }

    private render(): void {
        const ctx = this.ctx;
        const width = this.canvas.width;
        const height = this.canvas.height;

        ctx.clearRect(0, 0, width, height);

        const count = this.rxSeries.length;
        if (count === 0) {
            ctx.fillStyle = "#777777";
            ctx.font = "14px sans-serif";
            ctx.fillText("Waiting for data…", 12, height / 2);
            return;
        }

        let maxValue = 0;
        for (let i = 0; i < count; i++) {
            if (this.rxSeries[i] > maxValue) {
                maxValue = this.rxSeries[i];
            }
            if (this.txSeries[i] > maxValue) {
                maxValue = this.txSeries[i];
            }
        }
        if (maxValue <= 0) {
            maxValue = 1;
        }
        maxValue *= 1.1;

        this.drawGrid(maxValue);
        this.drawSeries(this.rxSeries, "#1f77b4", maxValue);
        this.drawSeries(this.txSeries, "#ff7f0e", maxValue);
    }

    private drawGrid(maxValue: number): void {
        const ctx = this.ctx;
        const width = this.canvas.width;
        const height = this.canvas.height;
        const steps = 4;

        ctx.save();
        ctx.strokeStyle = "#e0e0e0";
        ctx.lineWidth = 1;
        ctx.setLineDash([4, 4]);

        for (let i = 0; i <= steps; i++) {
            const y = height - (height * i / steps);
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(width, y);
            ctx.stroke();
        }

        ctx.restore();

        ctx.save();
        ctx.fillStyle = "#555555";
        ctx.font = "12px sans-serif";
        for (let i = 0; i <= steps; i++) {
            const value = maxValue * i / steps;
            const label = this.formatRate(value);
            const y = height - (height * i / steps) - 4;
            ctx.fillText(label, 6, y);
        }
        ctx.restore();
    }

    private drawSeries(series: number[], color: string, maxValue: number): void {
        const ctx = this.ctx;
        const width = this.canvas.width;
        const height = this.canvas.height;
        const count = series.length;
        if (count === 0) {
            return;
        }

        ctx.save();
        ctx.strokeStyle = color;
        ctx.lineWidth = 2;
        ctx.lineJoin = "round";
        ctx.lineCap = "round";

        ctx.beginPath();
        for (let i = 0; i < count; i++) {
            const x = count === 1 ? width : (width * i / (count - 1));
            const value = series[i];
            const ratio = maxValue > 0 ? value / maxValue : 0;
            const y = height - (height * ratio);
            if (i === 0) {
                ctx.moveTo(x, y);
            } else {
                ctx.lineTo(x, y);
            }
        }
        ctx.stroke();
        ctx.restore();
    }

    private formatRate(value: number): string {
        if (!isFinite(value) || value < 0) {
            return "0 B/s";
        }
        const units = ["B/s", "KiB/s", "MiB/s", "GiB/s", "TiB/s"];
        let idx = 0;
        let current = value;
        while (current >= 1024 && idx < units.length - 1) {
            current /= 1024;
            idx++;
        }
        const digits = idx === 0 ? 0 : (current < 10 ? 2 : 1);
        return current.toFixed(digits) + " " + units[idx];
    }
}

class LogsRateTable {
    private table: HTMLTableElement;

    constructor(table: HTMLTableElement) {
        this.table = table;
    }

    public render(sessions: LogRateSession[]): void {
        const header = [
            "<thead><tr>",
            "<th>ID</th>",
            "<th>Socket</th>",
            "<th>RX inst</th>",
            "<th>RX mean</th>",
            "<th>RX EWMA</th>",
            "<th>RX max</th>",
            "<th>RX total</th>",
            "<th>TX inst</th>",
            "<th>TX mean</th>",
            "<th>TX EWMA</th>",
            "<th>TX max</th>",
            "<th>TX total</th>",
            "</tr></thead>"
        ].join("");

        const bodyParts: string[] = [];
        bodyParts.push("<tbody>");
        if (sessions.length === 0) {
            bodyParts.push("<tr><td colspan=13 style='text-align:center;color:#777;'>No sessions</td></tr>");
        } else {
            for (let i = 0; i < sessions.length; i++) {
                const s = sessions[i];
                bodyParts.push("<tr>");
                bodyParts.push("<td>" + s.id + "</td>");
                bodyParts.push("<td>" + this.escapeHtml(s.socket) + "</td>");
                bodyParts.push("<td>" + this.formatRate(s.rx_inst) + "</td>");
                bodyParts.push("<td>" + this.formatRate(s.rx_mean) + "</td>");
                bodyParts.push("<td>" + this.formatRate(s.rx_ewma) + "</td>");
                bodyParts.push("<td>" + this.formatRate(s.rx_max) + "</td>");
                bodyParts.push("<td>" + this.formatBytes(s.rx_total) + "</td>");
                bodyParts.push("<td>" + this.formatRate(s.tx_inst) + "</td>");
                bodyParts.push("<td>" + this.formatRate(s.tx_mean) + "</td>");
                bodyParts.push("<td>" + this.formatRate(s.tx_ewma) + "</td>");
                bodyParts.push("<td>" + this.formatRate(s.tx_max) + "</td>");
                bodyParts.push("<td>" + this.formatBytes(s.tx_total) + "</td>");
                bodyParts.push("</tr>");
            }
        }
        bodyParts.push("</tbody>");

        this.table.innerHTML = header + bodyParts.join("");
    }

    private escapeHtml(value: string): string {
        return value.replace(/[&<>"]/g, (c) => {
            switch (c) {
                case "&": return "&amp;";
                case "<": return "&lt;";
                case ">": return "&gt;";
                case '"': return "&quot;";
                default: return c;
            }
        });
    }

    private formatRate(value: number): string {
        if (!isFinite(value) || value <= 0) {
            return "0 B/s";
        }
        const units = ["B/s", "KiB/s", "MiB/s", "GiB/s", "TiB/s"];
        let idx = 0;
        let current = value;
        while (current >= 1024 && idx < units.length - 1) {
            current /= 1024;
            idx++;
        }
        const digits = idx === 0 ? 0 : (current < 10 ? 2 : 1);
        return current.toFixed(digits) + " " + units[idx];
    }

    private formatBytes(value: number): string {
        if (!isFinite(value) || value <= 0) {
            return "0 B";
        }
        const units = ["B", "KiB", "MiB", "GiB", "TiB"];
        let idx = 0;
        let current = value;
        while (current >= 1024 && idx < units.length - 1) {
            current /= 1024;
            idx++;
        }
        const digits = idx === 0 ? 0 : (current < 10 ? 2 : 1);
        return current.toFixed(digits) + " " + units[idx];
    }
}

class LogsRatePage {
    private table: LogsRateTable;
    private chart: LogsRateChart;
    private summaryEl: HTMLElement | null;
    private statusEl: HTMLElement | null;
    private client: LogsRateClient;
    private lastError: string | null;

    constructor(document: Document) {
        const tableEl = document.getElementById("logs-rate-table") as HTMLTableElement;
        const canvas = document.getElementById("logs-rate-chart") as HTMLCanvasElement;
        this.summaryEl = document.getElementById("logs-rate-summary");
        this.statusEl = document.getElementById("logs-rate-status");

        this.table = new LogsRateTable(tableEl);
        this.chart = new LogsRateChart(canvas, 120);
        this.chart.resize();

        window.addEventListener("resize", () => {
            this.chart.resize();
        });

        this.lastError = null;

        this.client = new LogsRateClient(
            "/api/logs/rate",
            2000,
            (payload) => this.handleUpdate(payload),
            (error) => this.handleError(error)
        );
        this.client.start();
    }

    private handleUpdate(payload: LogRateResponse): void {
        const sessions = Array.isArray(payload.sessions) ? payload.sessions : [];
        this.table.render(sessions);

        let rxInstSum = 0;
        let txInstSum = 0;
        let rxTotal = 0;
        let txTotal = 0;
        let active = 0;

        for (let i = 0; i < sessions.length; i++) {
            const s = sessions[i];
            if (s.rx_inst > 0 || s.tx_inst > 0 || s.rx_total > 0 || s.tx_total > 0) {
                active += 1;
            }
            rxInstSum += this.safeNumber(s.rx_inst);
            txInstSum += this.safeNumber(s.tx_inst);
            rxTotal += this.safeNumber(s.rx_total);
            txTotal += this.safeNumber(s.tx_total);
        }

        this.chart.addSample(rxInstSum, txInstSum);

        if (this.summaryEl) {
            const parts: string[] = [];
            parts.push("Active sessions: " + active + " / " + sessions.length);
            parts.push("TX inst: " + this.formatRate(txInstSum));
            parts.push("RX inst: " + this.formatRate(rxInstSum));
            parts.push("TX total: " + this.formatBytes(txTotal));
            parts.push("RX total: " + this.formatBytes(rxTotal));
            this.summaryEl.textContent = parts.join("  •  ");
        }

        if (this.statusEl) {
            const now = new Date();
            this.statusEl.textContent = "Last update: " + now.toLocaleTimeString();
            this.statusEl.style.color = "#555555";
        }

        this.lastError = null;
    }

    private handleError(error: Error): void {
        this.lastError = error.message;
        if (this.statusEl) {
            this.statusEl.textContent = "Failed to load data: " + error.message;
            this.statusEl.style.color = "#b94a48";
        }
    }

    private safeNumber(value: number): number {
        if (!isFinite(value)) {
            return 0;
        }
        return value;
    }

    private formatRate(value: number): string {
        if (!isFinite(value) || value <= 0) {
            return "0 B/s";
        }
        const units = ["B/s", "KiB/s", "MiB/s", "GiB/s", "TiB/s"];
        let idx = 0;
        let current = value;
        while (current >= 1024 && idx < units.length - 1) {
            current /= 1024;
            idx++;
        }
        const digits = idx === 0 ? 0 : (current < 10 ? 2 : 1);
        return current.toFixed(digits) + " " + units[idx];
    }

    private formatBytes(value: number): string {
        if (!isFinite(value) || value <= 0) {
            return "0 B";
        }
        const units = ["B", "KiB", "MiB", "GiB", "TiB"];
        let idx = 0;
        let current = value;
        while (current >= 1024 && idx < units.length - 1) {
            current /= 1024;
            idx++;
        }
        const digits = idx === 0 ? 0 : (current < 10 ? 2 : 1);
        return current.toFixed(digits) + " " + units[idx];
    }
}
