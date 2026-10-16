"use strict";
(() => {
    const $ = id => document.getElementById(id);
    let socket, pending, drivers, saved, generation = 0;
    function rpc(driver, name, args = [], timeout = 5000) {
        if (pending) return Promise.reject(new Error("RPC already pending"));
        const command = driver.commands[name];
        const packet = new ArrayBuffer(8 + 4 * args.length), view = new DataView(packet);
        view.setUint16(4, driver.id); view.setUint16(6, command.id);
        args.forEach((value, i) => view.setUint32(8 + 4 * i, value));
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => { pending = null; reject(new Error("RPC timeout")); socket.close(); }, timeout);
            pending = {resolve, reject, timer, type: command.ret_type || command.retType};
            socket.send(packet);
        });
    }
    function render(report) {
        saved = report;
        let host = {};
        try { host = JSON.parse(report.host_results_json || "{}"); } catch (_) { /* Retain onboard results. */ }
        const checks = [...report.checks, ...(host.checks || [])];
        const counts = status => checks.filter(row => row.status === status).length;
        $("state").textContent = report.state;
        $("summary").textContent = `${counts("pass")} pass · ${counts("fail")} fail · ${counts("skipped")} skipped · ${report.observer_requests} report reads during the run`;
        $("start").disabled = report.state === "running";
        $("cancel").disabled = report.state !== "running";
        $("download").disabled = !report.run_id;
        $("checks").replaceChildren();
        checks.forEach(row => {
            const tr = document.createElement("tr");
            [row.status.toUpperCase(), row.group, row.name.replaceAll("_", " "),
                row.value === undefined || row.value === null ? "" : `${Number(row.value).toLocaleString(undefined, {maximumFractionDigits: 3})} ${row.unit || ""}`, row.detail].forEach((text, i) => {
                const td = document.createElement("td"); td.textContent = text;
                if (i === 0) td.className = row.status;
                tr.append(td);
            });
            $("checks").append(tr);
        });
        $("metadata").replaceChildren();
        for (const [key, value] of Object.entries(report.metadata)) {
            const dt = document.createElement("dt"), dd = document.createElement("dd");
            dt.textContent = key.replaceAll("_", " "); dd.textContent = typeof value === "object" ? JSON.stringify(value) : value;
            $("metadata").append(dt, dd);
        }
    }
    async function refresh(token) {
        if (token !== generation) return;
        try {
            if (!await rpc(drivers.FullCompletion, "wait", [60000], 65000)) {
                $("state").textContent = "Run has not completed; cancel or inspect its retained report";
            } else render(JSON.parse(await rpc(drivers.BistFull, "get_report")));
            if (token === generation) setTimeout(() => refresh(token), 500);
        } catch (error) {
            if (token === generation) { $("state").textContent = error.message; socket.close(); }
        }
    }
    function connect() {
        const token = ++generation;
        socket = new WebSocket(`${location.protocol === "https:" ? "wss" : "ws"}://${location.hostname}:8080`);
        socket.binaryType = "arraybuffer";
        socket.onmessage = event => {
            if (!pending) return;
            const call = pending; pending = null; clearTimeout(call.timer);
            const view = new DataView(event.data);
            call.resolve(call.type === "bool" ? view.getUint8(8) !== 0 : new TextDecoder().decode(new Uint8Array(event.data, 12, view.getUint32(8))));
        };
        socket.onopen = async () => {
            try {
                const context = JSON.parse(await rpc({id: 1, commands: {context: {id: 1, retType: "std::string"}}}, "context"));
                drivers = Object.fromEntries(context.map(item => [item.class || item.name, {
                    id: item.id, commands: Object.fromEntries((item.functions || item.commands).map(command => [command.name, command]))
                }]));
                if (!drivers.BistFull || !drivers.FullCompletion) throw new Error("BIST Full instrument is required");
                $("state").textContent = "Waiting for the current run to finish…";
                if (token === generation) refresh(token);
            } catch (error) { $("state").textContent = error.message; socket.close(); }
        };
        socket.onclose = () => {
            if (token !== generation) return;
            ++generation;
            $("start").disabled = true; $("cancel").disabled = true;
            if (pending) { clearTimeout(pending.timer); pending.reject(new Error("Disconnected")); pending = null; }
            setTimeout(connect, 1000);
        };
    }
    // Controls use their own session while the completion socket is sleeping.
    function control(name, args) {
        return new Promise((resolve, reject) => {
            const control = new WebSocket(socket.url); control.binaryType = "arraybuffer";
            const timer = setTimeout(() => { control.close(); reject(new Error("Control timeout")); }, 5000);
            control.onopen = () => {
                const packet = new ArrayBuffer(8 + 4 * args.length), view = new DataView(packet);
                view.setUint16(4, drivers.BistFull.id); view.setUint16(6, drivers.BistFull.commands[name].id);
                args.forEach((value, i) => view.setUint32(8 + 4 * i, value)); control.send(packet);
                if (name === "cancel") { clearTimeout(timer); setTimeout(() => { control.close(); resolve(true); }, 100); }
            };
            control.onmessage = event => { clearTimeout(timer); control.close(); resolve(new DataView(event.data).getUint8(8) !== 0); };
            control.onerror = () => { clearTimeout(timer); control.close(); reject(new Error("Control connection failed")); };
        });
    }
    $("start").onclick = async () => {
        $("start").disabled = true;
        try {
            if (!await control("start", [Number($("mask").value), 35000, 0xb157f011])) throw new Error("Run could not start");
            $("state").textContent = "Running quietly; results appear after completion";
            $("cancel").disabled = false; $("download").disabled = true;
        } catch (error) { $("state").textContent = error.message; }
    };
    $("cancel").onclick = () => control("cancel", []).catch(error => { $("state").textContent = error.message; });
    $("download").onclick = () => {
        const url = URL.createObjectURL(new Blob([JSON.stringify(saved, null, 2)], {type: "application/json"}));
        const a = document.createElement("a"); a.href = url; a.download = `bist-full-${saved.run_id}.json`; a.click(); URL.revokeObjectURL(url);
    };
    connect();
})();
