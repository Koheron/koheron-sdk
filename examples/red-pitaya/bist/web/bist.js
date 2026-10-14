/* Generated SDK RPC protocol over WebSocket. One request outstanding per socket. */
"use strict";
(() => {
    const $ = id => document.getElementById(id);
    let socket, pending, driver, commands, report, cursor = 0, runId = "", connecting = false, generation = 0;
    function rpc(name, args = []) {
        if (pending) return Promise.reject(new Error("RPC already pending"));
        const command = commands[name];
        const packet = new ArrayBuffer(8 + args.length * 4), view = new DataView(packet);
        view.setUint16(4, driver.id); view.setUint16(6, command.id);
        args.forEach((value, index) => view.setUint32(8 + index * 4, value));
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => { pending = null; reject(new Error("RPC timeout")); socket.close(); }, 5000);
            pending = {resolve, reject, timer, type: command.ret_type || command.retType};
            socket.send(packet);
        });
    }
    function render() {
        if (!report) return;
        $("state").textContent = report.state;
        $("start").disabled = report.state === "running";
        $("cancel").disabled = report.state !== "running";
        $("download").disabled = !report.run_id;
        $("outcomes").textContent = `Functional: ${report.functional_status || "unavailable"} · Performance: ${(report.performance?.status || "not evaluated").replaceAll("_", " ")}`;
        $("summary").textContent = report.run_id ? `Run ${report.run_id} · ${report.metrics.length} measurements · reports stored onboard` : "Ready to run";
        const metadata = window.bistMetadata.rows(report.metadata);
        for (const [id, rows] of [["metadata", metadata.summary], ["metadata-details", metadata.details]]) {
            const list = $(id); list.replaceChildren();
            for (const [label, value] of rows) {
                const entry = document.createElement("div"), term = document.createElement("dt"), definition = document.createElement("dd");
                term.textContent = label; definition.textContent = value;
                entry.append(term, definition); list.append(entry);
            }
        }
        $("performance-metrics").replaceChildren();
        let comparison = {};
        try { comparison = JSON.parse(report.performance?.comparison_json || "{}"); } catch (_) { /* Invalid saved comparison. */ }
        $("performance-details").hidden = !comparison.metrics?.length;
        (comparison.metrics || []).forEach(row => {
            const tr = document.createElement("tr");
            [row.name.replaceAll("_", " "), row.status, row.value === undefined ? "Unavailable" : `${Number(row.value).toFixed(3)} ${row.unit}`,
                row.threshold === undefined ? "Unavailable" : Number(row.threshold).toFixed(3)].forEach(text => {
                const td = document.createElement("td"); td.textContent = text; tr.append(td);
            });
            $("performance-metrics").append(tr);
        });
        $("metrics").replaceChildren();
        report.metrics.forEach(row => {
            const tr = document.createElement("tr"), status = row.status === "skipped" ? "SKIP" : row.status === "incomplete" ? "INCOMPLETE" : row.pass ? "PASS" : "FAIL";
            [status, row.domain === "os" ? "OS validation" : "Instrument health", row.name.replaceAll("_", " "),
                row.value === null ? "Unavailable" : `${Number(row.value).toLocaleString(undefined, {maximumFractionDigits: 3})} ${row.unit}`].forEach((text, index) => {
                const td = document.createElement("td"); td.textContent = text;
                if (index === 0) td.className = status === "SKIP" ? "skipped" : row.pass ? "pass" : "fail";
                tr.append(td);
            });
            tr.title = row.detail; $("metrics").append(tr);
        });
    }
    async function poll(token) {
        if (token !== generation) return;
        try {
            let batch = JSON.parse(await rpc("get_log", [cursor]));
            if (batch.run_id !== runId) {
                runId = batch.run_id; cursor = 0; $("log").textContent = "";
                batch = JSON.parse(await rpc("get_log", [0]));
            }
            const follow = $("log").scrollHeight - $("log").scrollTop - $("log").clientHeight < 40;
            $("log").textContent += batch.text; cursor = batch.cursor;
            if (follow) $("log").scrollTop = $("log").scrollHeight;
            report = JSON.parse(await rpc("get_report")); render();
            setTimeout(() => poll(token), 300);
        } catch (error) { $("state").textContent = error.message; socket.close(); }
    }
    function connect() {
        if (connecting) return; connecting = true;
        const token = ++generation;
        socket = new WebSocket(`${location.protocol === "https:" ? "wss" : "ws"}://${location.hostname}:8080`);
        socket.binaryType = "arraybuffer";
        socket.onopen = async () => {
            commands = {context: {id: 1, retType: "std::string"}}; driver = {id: 1};
            try {
                const context = JSON.parse(await rpc("context"));
                driver = context.find(item => (item.class || item.name) === "Bist");
                if (!driver) throw new Error("BIST driver is unavailable");
                commands = Object.fromEntries((driver.functions || driver.commands).map(command => [command.name, command]));
                connecting = false; poll(token);
            } catch (error) { $("state").textContent = error.message; socket.close(); }
        };
        socket.onmessage = event => {
            if (!pending) return;
            const call = pending; pending = null; clearTimeout(call.timer);
            const view = new DataView(event.data);
            if (call.type === "bool") call.resolve(view.getUint8(8) !== 0);
            else call.resolve(new TextDecoder().decode(new Uint8Array(event.data, 12, view.getUint32(8))));
        };
        socket.onclose = () => {
            ++generation;
            connecting = false; $("state").textContent = "Disconnected · reconnecting…";
            $("start").disabled = true; $("cancel").disabled = true;
            if (pending) { clearTimeout(pending.timer); pending.reject(new Error("WebSocket disconnected")); pending = null; }
            setTimeout(connect, 1000);
        };
    }
    // Use a separate RPC socket for control so the log reader stays responsive.
    async function control(name, args) {
        const controlSocket = new WebSocket(socket.url); controlSocket.binaryType = "arraybuffer";
        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {controlSocket.close(); reject(new Error("Control timeout"));}, 5000);
            controlSocket.onopen = () => {
                const packet = new ArrayBuffer(8 + args.length * 4), view = new DataView(packet);
                view.setUint16(4, driver.id); view.setUint16(6, commands[name].id);
                args.forEach((value, i) => view.setUint32(8 + i*4, value)); controlSocket.send(packet);
                if (name === "cancel") { clearTimeout(timer); setTimeout(() => {controlSocket.close(); resolve(true);}, 100); }
            };
            controlSocket.onmessage = event => { clearTimeout(timer); controlSocket.close(); resolve(new DataView(event.data).getUint8(8) !== 0); };
            controlSocket.onerror = () => { clearTimeout(timer); reject(new Error("Control connection failed")); };
        });
    }
    $("start").onclick = async () => {
        $("start").disabled = true;
        try { if (!await control("start_profile", [Number($("mask").value), Number($("profile").value)])) $("state").textContent = "Run could not start"; }
        catch (error) { $("state").textContent = error.message; }
    };
    $("cancel").onclick = () => control("cancel", []).catch(error => { $("state").textContent = error.message; });
    $("download").onclick = () => {
        const url = URL.createObjectURL(new Blob([JSON.stringify(report, null, 2)], {type: "application/json"}));
        const a = document.createElement("a"); a.href = url; a.download = `bist-${report.run_id}.json`; a.click(); URL.revokeObjectURL(url);
    };
    connect();
})();
