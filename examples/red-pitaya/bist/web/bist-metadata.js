"use strict";
(() => {
    const available = value => value === undefined || value === null || value === "" ? "Unavailable" : String(value).trim();
    function assignments(text = "") {
        return Object.fromEntries(text.split(/\r?\n/).filter(line => /^[A-Z_]+=/.test(line)).map(line => {
            const index = line.indexOf("="), key = line.slice(0, index);
            let value = line.slice(index + 1);
            if ((value.startsWith('"') && value.endsWith('"')) || (value.startsWith("'") && value.endsWith("'"))) value = value.slice(1, -1);
            return [key, value];
        }));
    }
    function rows(metadata = {}) {
        const os = metadata.os || assignments(metadata.os_release);
        const image = metadata.os_image || {};
        let instrument = {};
        try { instrument = JSON.parse(metadata.instrument_manifest || "{}"); } catch (_) { /* Older or interrupted report. */ }
        return {
            summary: [
                ["Instrument", `${metadata.instrument_name || "bist"} ${available(metadata.instrument_version || metadata.bist_version)}`],
                ["SDK version", available(metadata.sdk_version)],
                ["Operating system", available(os.PRETTY_NAME)],
                ["Kernel", available(metadata.kernel)],
                ["OS image", available(image.RELEASE)],
                ["Board", available(metadata.board)],
                ["Profile", available(metadata.parameters?.profile)]
            ],
            details: [
                ["Run started (UTC)", available(metadata.started_utc)],
                ["OS build", available(image.BUILD_ID)],
                ["OS source", available(image.GIT_DESCRIBE || image.GIT_COMMIT)],
                ["SDK source", available(instrument.sdk_commit)],
                ["Workload version", available(metadata.workload_version)],
                ["Boot ID", available(metadata.boot_id)],
                ["FPGA identity", available(metadata.fpga_identity)],
                ["FPGA DNA", available(metadata.fpga_dna)],
                ["FPGA SHA256", available(metadata.fpga_sha256)],
                ["Server SHA256", available(metadata.server_sha256)],
                ["BIST source SHA256", available(metadata.bist_build_id)]
            ]
        };
    }
    window.bistMetadata = {rows};
})();
