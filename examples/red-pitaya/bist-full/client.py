#!/usr/bin/env python3
"""Full Red Pitaya validation; one hard 60-second end-to-end deadline."""

import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import closing
import hashlib
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import threading
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "python"))
import numpy as np
import requests
from koheron import KoheronClient, command

GROUPS = {
    "platform",
    "memory",
    "ddr",
    "cpu",
    "identity",
    "storage",
    "dma",
    "irq",
    "uart",
    "analog",
    "stress",
}


def required_checks(mask):
    """The coverage contract, independent of a run's completion markers."""
    cases = {
        "platform": (
            "mapping_backends fpga_identity dma_reservation mapping_lifetime_32 "
            "read_only_write_denied converter_clock fpga_temperature pl_vccint "
            "pl_vccaux pl_vccbram ps_vccint ps_vccaux ps_vccmem"
        ).split(),
        "memory": ["register_patterns", "snapshot_token_wrap"],
        "dma": (
            "uio_binding pl_generator_rx_only pl_checker_tx_only duplex_2m_capacity "
            "reject_zero_length_bd reject_completed_bd descriptor_fault_1_recovery "
            "descriptor_fault_2_recovery randomized_asymmetric_sg detect_tx_corruption tx_corruption_recovery "
            "detect_post_checker_corruption output_corruption_recovery detect_early_tlast "
            "detect_missing_tlast tlast_2_recovery tlast_4_recovery masked_uio_irq_timeout "
            "masked_irq_recovery reject_invalid_requests capture_overflow_recovery"
        ).split(),
        "ddr": (
            "cpu_write_pl_read detect_cpu_corruption corruption_recovery "
            "write_strobe_isolation complementary_write_strobes hardware_address_fence "
            "draining_abort abort_recovery retention_prepare retention_after_load"
        ).split(),
        "cpu": ["core_0", "core_1", "cacheable_32m", "cache_line_handoff"],
        "identity": ["factory_eeprom", "ethernet_factory_mac", "ethernet_link"],
        "storage": ["atomic_file", "detect_media_corruption", "cleanup"],
        "irq": ["level_rearm_32", "masked_level_retention"],
        "uart": (
            "uio_binding idle_timeout request_reply_100 detect_rx_overrun "
            "detect_tx_overrun overrun_recovery"
        ).split(),
        "analog": ["detect_capture_overflow"],
        "stress": ["cpu_dma_uart"],
        "lifecycle": ["onboard_budget"],
    }
    for memory in ("bram16k", "ddr8m"):
        cases["memory"] += [
            f"{memory}_{name}"
            for name in (
                "march_c_minus",
                "walking_data",
                "walking_address",
                "byte_halfword_strobes",
                "address_pattern",
            )
        ]
    for prefix, values in (
        (
            "bytes",
            (
                1,
                2,
                3,
                4,
                5,
                7,
                8,
                15,
                16,
                31,
                32,
                63,
                64,
                65,
                127,
                128,
                255,
                256,
                511,
                512,
                1023,
                1024,
                4095,
                4096,
                4097,
                65535,
                65536,
                65537,
            ),
        ),
        ("unaligned", (1, 2, 3, 7, 15, 63, 4093, 4095)),
        ("sg", (2, 3, 31, 32, 63, 64)),
        ("backpressure", (128, 32768, 61680)),
        ("descriptor_reuse", range(16)),
    ):
        cases["dma"] += [f"{prefix}_{value}" for value in values]
    cases["ddr"] += [
        f"{pattern}_{operation}"
        for pattern in (
            "zeros",
            "ones",
            "checkerboard",
            "address",
            "walking_one",
            "walking_zero",
            "scrambled",
        )
        for operation in ("write_128m", "read_128m", "cpu_crosscheck")
    ]
    cases["ddr"] += [f"burst_{burst}_4k_crossing" for burst in (1, 16, 64, 256)]
    cases["uart"] += ["framing_break_0", "framing_break_1"]
    cases["uart"] += [f"duplex_bytes_{n}" for n in (1, 31, 32, 63, 64, 65, 127, 128, 129, 32768)]
    for channel in range(2):
        if mask & (1 << channel):
            cases["analog"] += [
                f"ch{channel}_bin{b}_amp{a}_dc{dc}"
                for b, a, dc in (
                    (8, 512, 0),
                    (8, 2048, 0),
                    (8, 4096, 0),
                    (32, 4096, 0),
                    (64, 2048, 0),
                    (8, 2048, 500),
                    (8, 2048, -500),
                )
            ]
            cases["analog"] += [f"ch{channel}_spectrum_{b}" for b in (13, 37, 131, 211)]
            cases["analog"] += [
                f"ch{channel}_{name}"
                for name in (
                    "dc_transfer",
                    "high_frequency_headroom",
                    "amplitude_linearity",
                    "adc_bit_activity",
                    "other_dac_isolation",
                    "detect_clipped_waveform",
                    "clipping_recovery",
                    "two_tone_imd",
                    "step_response",
                    "prbs_correlation",
                )
            ]
        else:
            cases["analog"].append(f"channel_{channel}")
    return {(group, name) for group, names in cases.items() for name in names}


class FullClient:
    def __init__(self, host, timeout=3):
        self.client = KoheronClient(host, timeout=timeout)

    def close(self):
        self.client.sock.close()

    @command(classname="BistFull")
    def start(self, analog_mask, budget_ms, seed):
        return self.client.recv_bool()

    @command(classname="BistFull")
    def cancel(self):
        pass

    @command(classname="FullCompletion")
    def wait(self, timeout_ms):
        return self.client.recv_bool()

    @command(classname="BistFull")
    def get_report(self):
        return self.client.recv_json()

    @command(classname="BistFull")
    def get_artifact(self, run_id, name):
        return self.client.recv_json()

    @command(classname="BistFull")
    def ping(self, value):
        return self.client.recv_uint32()

    @command(classname="BistFull")
    def echo_u64(self, value):
        return self.client.recv_uint64()

    @command(classname="BistFull")
    def echo_double(self, value):
        return self.client.recv_double()

    @command(classname="BistFull")
    def echo_vector(self, values):
        return self.client.recv_vector()

    @command(classname="BistFull")
    def recover_devices(self):
        return self.client.recv_bool()

    @command(classname="BistFull")
    def record_host_results(self, run_id, results_json):
        return self.client.recv_bool()


def remaining(deadline, maximum=3):
    seconds = deadline - time.monotonic()
    if seconds <= 0:
        raise TimeoutError("60-second validation deadline exceeded; partial results retained")
    return min(seconds, maximum)


def connect(host, deadline):
    while time.monotonic() < deadline:
        connection = None
        try:
            connection = FullClient(host, remaining(deadline))
            connection.client.sock.settimeout(remaining(deadline))
            if connection.ping(0x1234) == (0x1234 ^ 0x42494631):
                return connection
        except (OSError, ValueError, KeyError, requests.RequestException):
            pass
        if connection:
            connection.close()
        time.sleep(min(0.05, remaining(deadline)))
    raise TimeoutError("Full instrument RPC readiness deadline exceeded")


def api(host, route, deadline, post=False):
    response = requests.request(
        "POST" if post else "GET", f"http://{host}/api/{route}", timeout=remaining(deadline, 5)
    )
    response.raise_for_status()
    return response.json()


def reboot(args, deadline):
    import paramiko

    api(args.host, "instruments/default/bist-full", deadline, True)
    with paramiko.SSHClient() as remote:
        if args.ssh_known_hosts:
            remote.load_host_keys(args.ssh_known_hosts)
        else:
            remote.load_system_host_keys()
        # Follow the existing BIST transport for first-use boards. A selected
        # key file still rejects any mismatch rather than replacing its key.
        remote.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        remote.connect(
            args.host,
            username="root",
            password=os.environ.get("BIST_SSH_PASSWORD", "changeme"),
            look_for_keys=True,
            allow_agent=True,
            timeout=remaining(deadline, 5),
        )
        _, stdout, _ = remote.exec_command(
            "cat /proc/sys/kernel/random/boot_id", timeout=remaining(deadline)
        )
        old_boot = stdout.read().decode().strip()
        started = time.monotonic()
        remote.exec_command("reboot", timeout=remaining(deadline))
    lost = False
    while time.monotonic() < deadline:
        reachable = (
            subprocess.run(
                ["ping", "-n", "-c", "1", "-W", "0.2", args.host],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=remaining(deadline, 1),
            ).returncode
            == 0
        )
        if not reachable:
            lost = True
        if reachable and lost:
            return old_boot, started, time.monotonic() - started
        time.sleep(min(0.05, remaining(deadline)))
    raise TimeoutError("Network did not return after reboot")


def collect(connection, deadline):
    seconds = remaining(deadline, 60)
    sock = connection.client.sock
    previous = sock.gettimeout()
    try:
        sock.settimeout(seconds)
        done = connection.wait(max(0, int(seconds * 1000) - 50))
    finally:
        sock.settimeout(previous)
    if not done:
        connection.cancel()
        raise TimeoutError("Onboard deadline exceeded; partial report remains on the board")
    return connection.get_report()


def validate_report(report):
    errors = []
    if report.get("instrument") != "bist-full" or report.get("state") != "complete":
        errors.append("incomplete or incorrect instrument report")
    if not report.get("storage_ok"):
        errors.append("unhealthy report storage")
    if set(report.get("completed_groups", [])) != GROUPS:
        errors.append("missing coverage groups")
    rows = report.get("checks", [])
    if not rows or len({(r["group"], r["name"]) for r in rows}) != len(rows):
        errors.append("missing or duplicate checks")
    mask = report.get("metadata", {}).get("parameters", {}).get("analog_mask")
    if type(mask) is not int or mask not in range(4):
        errors.append("missing or invalid wiring declaration")
    else:
        missing = required_checks(mask) - {(r["group"], r["name"]) for r in rows}
        errors.extend(f"missing check: {group}/{name}" for group, name in sorted(missing))
    allowed_skips = {
        ("analog", f"channel_{channel}")
        for channel in range(2)
        if type(mask) is int and not mask & (1 << channel)
    }
    for row in rows:
        if row.get("status") not in ("pass", "skipped"):
            errors.append(f"{row['group']}/{row['name']}: {row.get('status')}")
        if row.get("status") == "skipped" and (row["group"], row["name"]) not in allowed_skips:
            errors.append(f"unexpected skip: {row['group']}/{row['name']}")
        if (row["group"], row["name"]) in allowed_skips and row.get("status") != "skipped":
            errors.append(f"unconnected channel must be explicitly skipped: {row['name']}")
    for key in ("source_sha256", "fpga_sha256", "server_sha256", "fpga_dna", "boot_id"):
        if report.get("metadata", {}).get(key) in (None, "", "unavailable"):
            errors.append(f"missing identity: {key}")
    return errors


def protocol_checks(host, connection, deadline, checks):

    def invoke(method, *args):
        connection.client.sock.settimeout(remaining(deadline))
        return method(*args)

    def add(name, ok, detail, **values):
        checks.append(
            dict(group="sdk", name=name, status="pass" if ok else "fail", detail=detail, **values)
        )

    for name, mask, budget in (
        ("invalid_wiring_mask", 4, 1000),
        ("invalid_short_budget", 1, 999),
        ("invalid_long_budget", 1, 35001),
    ):
        add(
            name,
            not invoke(connection.start, mask, budget, 1),
            "invalid control arguments rejected without starting a run",
        )
    for value in (0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
        connection.client.sock.settimeout(remaining(deadline))
        add(
            f"u32_{value}",
            invoke(connection.ping, value) == (value ^ 0x42494631),
            "unsigned 32-bit boundary roundtrip",
        )
    for value in (0, 1, 2**32 - 1, 2**32, 2**63, 2**64 - 1):
        add(
            f"u64_{value}",
            invoke(connection.echo_u64, value) == value,
            "unsigned 64-bit boundary roundtrip",
        )
    for index, value in enumerate(
        (0.0, -0.0, 1.0, -1.0, 1e-200, 1e200, float("inf"), -float("inf"))
    ):
        got = invoke(connection.echo_double, value)
        add(
            f"double_{index}",
            struct.pack("!d", got) == struct.pack("!d", value),
            "exact IEEE754 bits, including signed zero and infinities",
        )
    for size in (0, 1, 2, 255, 256, 257, 1023, 1024, 4096, 16384):
        values = np.arange(size, dtype=np.uint32) * np.uint32(2654435761)
        got = invoke(connection.echo_vector, values)
        add(
            f"vector_{size}",
            np.array_equal(got, values),
            "empty, boundary and 64 KiB generated vector roundtrips",
        )

    device, operation, _ = connection.client.get_ids("BistFull", "echo_vector")
    header = struct.pack("!IHH", 0, device, operation)
    with socket.create_connection((host, 36000), timeout=remaining(deadline)) as stalled:
        stalled.sendall(header + struct.pack("!I", 1024) + b"\0\0\0\1")
        # The decoder must not hold the driver lock while awaiting arguments.
        started = time.monotonic()
        responsive = invoke(connection.ping, 7) == (7 ^ 0x42494631)
        elapsed = time.monotonic() - started
        add(
            "partial_vector_does_not_lock_driver",
            responsive and elapsed < 0.5,
            "another client stalls partway through its body",
            ping_seconds=elapsed,
        )
    for name, packet in (
        ("invalid_device", struct.pack("!IHH", 0, 65535, 0)),
        ("invalid_operation", struct.pack("!IHH", 0, device, 65535)),
        ("partial_header_eof", b"\0\0\0"),
    ):
        with socket.create_connection((host, 36000), timeout=remaining(deadline)) as raw:
            raw.settimeout(remaining(deadline, 0.5))
            raw.sendall(packet)
            raw.shutdown(socket.SHUT_WR)
            try:
                closed = raw.recv(16) == b""
            except ConnectionResetError:
                closed = True
            except TimeoutError:
                closed = False
        add(
            name,
            closed and invoke(connection.ping, 8) == (8 ^ 0x42494631),
            "bad session closes and the healthy connection remains usable",
        )

    def receive(raw, n):
        data = bytearray()
        while len(data) < n:
            raw.settimeout(remaining(deadline))
            part = raw.recv(n - len(data))
            if not part:
                raise ConnectionError("early EOF in protocol probe")
            data.extend(part)
        return bytes(data)

    ping_device, ping_operation, _ = connection.client.get_ids("BistFull", "ping")

    def ping_packet(value):
        return struct.pack("!IHHI", 0, ping_device, ping_operation, value)

    with socket.create_connection((host, 36000), timeout=remaining(deadline)) as raw:
        raw.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        for byte in ping_packet(0xFEDCBA98):
            raw.sendall(bytes([byte]))
        response = receive(raw, 12)
        add(
            "fragmented_request",
            struct.unpack("!I", response[8:])[0] == (0xFEDCBA98 ^ 0x42494631),
            "one request split into single-byte writes, including header and argument",
        )
        values = [i * 0x1020304 & 0xFFFFFFFF for i in range(128)]
        raw.sendall(b"".join(ping_packet(v) for v in values))
        response = receive(raw, 12 * len(values))
        add(
            "pipelined_requests",
            all(
                struct.unpack_from("!I", response, i * 12 + 8)[0] == (v ^ 0x42494631)
                for i, v in enumerate(values)
            ),
            "128 coalesced requests with ordered, exact replies on one connection",
        )
    for name, packet in (
        ("non_element_vector_length", header + struct.pack("!I", 3) + b"abc"),
        ("oversized_vector_length", header + struct.pack("!I", 0xFFFFFFFF)),
    ):
        with socket.create_connection((host, 36000), timeout=remaining(deadline)) as raw:
            raw.settimeout(remaining(deadline, 0.5))
            raw.sendall(packet)
            raw.shutdown(socket.SHUT_WR)
            try:
                closed = raw.recv(16) == b""
            except ConnectionResetError:
                closed = True
            except TimeoutError:
                closed = False
        add(
            name,
            closed and invoke(connection.ping, 9) == (9 ^ 0x42494631),
            "invalid dynamic-payload length closes only the offending session",
        )

    def concurrent(index):
        with closing(connect(host, deadline)) as other:
            values = np.arange(257, dtype=np.uint32) ^ np.uint32(index * 0x1010101)
            for _ in range(20):
                other.client.sock.settimeout(remaining(deadline))
                if not np.array_equal(other.echo_vector(values), values):
                    return False
            return True

    with ThreadPoolExecutor(max_workers=4) as pool:
        outcomes = list(pool.map(concurrent, range(4)))
    add(
        "four_concurrent_clients",
        all(outcomes),
        "80 vector roundtrips, distinct per-client payloads and independent connections",
    )
    run_id = invoke(connection.get_report)["run_id"]
    rejected = 0
    for identity, name in (
        (run_id, "../checkpoint"),
        (run_id, "ch0_../checkpoint.json"),
        (run_id, "ch0_\0.json"),
        ("wrong-run", "ch_manifest.json"),
    ):
        try:
            invoke(connection.get_artifact, identity, name)
        except ValueError:
            rejected += 1
    add(
        "artifact_access_validation",
        rejected == 4 and invoke(connection.ping, 10) == (10 ^ 0x42494631),
        "traversal, embedded NUL and wrong-run artifact requests return no data; connection remains usable",
    )
    return checks


def collect_with_traffic(host, connection, deadline):
    stop = threading.Event()

    def worker(index):
        count, errors, latencies = 0, [], []
        try:
            with closing(connect(host, deadline)) as other:
                while not stop.is_set():
                    other.client.sock.settimeout(remaining(deadline))
                    values = np.arange(1024, dtype=np.uint32) ^ np.uint32(
                        (index << 24) | (count & 0xFFFFFF)
                    )
                    when = time.monotonic()
                    if not np.array_equal(other.echo_vector(values), values):
                        errors.append("payload mismatch")
                    latencies.append((time.monotonic() - when) * 1000)
                    count += 1
                    stop.wait(0.01)
        except (OSError, ValueError, RuntimeError) as error:
            errors.append(str(error))
        return dict(calls=count, errors=errors, max_ms=max(latencies, default=0))

    with ThreadPoolExecutor(max_workers=4) as pool:
        pending = [pool.submit(worker, i) for i in range(4)]
        try:
            onboard = collect(connection, deadline)
        finally:
            stop.set()
        results = [future.result() for future in pending]
    return onboard, dict(
        group="sdk",
        name="traffic_during_hardware",
        status="pass" if all(r["calls"] > 0 and not r["errors"] for r in results) else "fail",
        detail="four paced clients verify distinct 4 KiB vectors throughout onboard hardware tests; no report polling",
        workers=results,
        payload_bytes_per_call=4096,
        period_seconds=0.01,
    )


def download_artifacts(connection, onboard, output, deadline):
    connection.client.sock.settimeout(remaining(deadline))
    manifest = connection.get_artifact(onboard["run_id"], "ch_manifest.json")
    expected = 21 * onboard["metadata"]["parameters"]["analog_mask"].bit_count()
    if len(manifest["files"]) != expected or len(set(manifest["files"])) != expected:
        raise RuntimeError("missing or duplicated analog evidence artifacts")
    target = output / "captures"
    target.mkdir(exist_ok=True)
    for name in manifest["files"]:
        if Path(name).name != name or not name.startswith("ch") or not name.endswith(".json"):
            raise RuntimeError("unsafe evidence artifact name")
        connection.client.sock.settimeout(remaining(deadline))
        data = connection.get_artifact(onboard["run_id"], name)
        if len(data["adc0"]) != 8192 or len(data["adc1"]) != 8192 or len(data["dac"]) != 1024:
            raise RuntimeError("incomplete analog evidence artifact")
        (target / name).write_text(json.dumps(data, separators=(",", ":")) + "\n")
    comparisons = []
    for row in onboard["checks"]:
        if row["group"] != "analog" or "_spectrum_" not in row["name"]:
            continue
        raw = json.loads((target / row["capture_artifact"]).read_text())
        y = np.asarray(raw[f"adc{raw['observe_channel']}"], dtype=float)
        p = np.abs(np.fft.rfft(y)) ** 2 / len(y) ** 2
        p[1:-1] *= 2
        b = round(row["frequency_hz"] * len(y) / raw["sample_rate_hz"])
        harmonic_bins = {(h * b) % len(y) for h in range(2, 11)}
        harmonic_bins = {min(i, len(y) - i) for i in harmonic_bins} - {0, b}
        signal, rest = p[b], sum(p[1:]) - p[b]
        distortion = sum(p[i] for i in harmonic_bins)
        independent = dict(
            amplitude=np.sqrt(2 * signal),
            sndr_db=10 * np.log10(max(signal, 1e-30) / max(rest, 1e-30)),
            thd_dbc=10 * np.log10(max(distortion, 1e-30) / max(signal, 1e-30)),
            sfdr_db=10 * np.log10(max(signal, 1e-30) / max(max(np.delete(p[1:], b - 1)), 1e-30)),
        )
        comparisons.append(
            dict(
                name=row["name"],
                match=all(
                    np.isclose(row[k], v, rtol=1e-6, atol=1e-6) for k, v in independent.items()
                ),
                numpy=independent,
            )
        )
    (output / "spectral-crosscheck.json").write_text(json.dumps(comparisons, indent=2) + "\n")
    return dict(
        group="evidence",
        name="analog_captures",
        status=(
            "pass"
            if comparisons and all(row["match"] for row in comparisons) or expected == 0
            else "fail"
        ),
        value=expected,
        unit="raw captures",
        detail="DAC codes and both ADC channels retained; NumPy independently verifies onboard amplitude, SNDR, THD and SFDR",
    )


def save(output, onboard, host):
    output.mkdir(parents=True, exist_ok=True)
    report = dict(onboard=onboard, host=host)
    (output / "report.json").write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
    (output / "onboard.json").write_text(json.dumps(onboard, indent=2, allow_nan=False) + "\n")
    lines = [f"bist-full | run {onboard['run_id']} | state {onboard['state']}"]
    for row in onboard["checks"] + host.get("checks", []):
        lines.append(
            f"{row['status'].upper():7} {row['group']}/{row['name']}: {row.get('value', '')} {row.get('unit', '')} | {row['detail']}"
        )
    (output / "scorecard.txt").write_text("\n".join(lines) + "\n")


def validate(args):
    started = time.monotonic()
    deadline = started + 60
    host = {
        "budget_seconds": 60,
        "client_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "checks": [],
    }
    startup = reboot(args, deadline) if not args.no_reboot else None
    with closing(connect(args.host, deadline)) as connection:
        if startup:
            host["startup"] = dict(
                network_seconds=startup[2],
                rpc_seconds=time.monotonic() - startup[1],
                old_boot_id=startup[0],
            )
        budget = min(35000, int((remaining(deadline, 60) - 7) * 1000))
        if not connection.start(args.analog_mask, budget, args.seed):
            raise RuntimeError("Full BIST start rejected")
        print(f"Onboard validation running quietly; allowance {budget/1000:.1f} s", flush=True)
        try:
            onboard, traffic = collect_with_traffic(args.host, connection, deadline - 6)
            host["checks"].append(traffic)
        except (OSError, TimeoutError):
            # Use an independent session: a timed-out completion RPC may still
            # be outstanding on its original socket.
            with closing(connect(args.host, deadline)) as rescue:
                rescue.cancel()
                onboard = rescue.get_report()
                save(args.output, onboard, host)
            raise
        save(args.output, onboard, host)
        errors = validate_report(onboard)
        if onboard["state"] == "complete":
            host["checks"].append(download_artifacts(connection, onboard, args.output, deadline))
        if startup and onboard["metadata"]["boot_id"].strip() == startup[0]:
            errors.append("reboot did not change boot ID")
        try:
            protocol_checks(args.host, connection, deadline, host["checks"])
        finally:
            save(args.output, onboard, host)
        host["checks"].append(
            dict(
                group="coverage",
                name="onboard_suite",
                status="fail" if errors else "pass",
                detail="; ".join(errors) or "all required onboard groups complete and healthy",
            )
        )
        save(args.output, onboard, host)
    restart = time.monotonic()
    api(args.host, "instruments/control/stop", deadline, True)
    api(args.host, "instruments/control/start", deadline, True)
    with closing(connect(args.host, deadline)) as recovered:
        restored = recovered.get_report()
        identical = (
            restored["run_id"] == onboard["run_id"]
            and restored["checks"] == onboard["checks"]
            and restored["storage_ok"]
        )
        host["checks"].append(
            dict(
                group="lifecycle",
                name="service_restart_retention",
                status="pass" if identical else "fail",
                detail="overlay/service restart retains the exact completed checkpoint",
                value=time.monotonic() - restart,
                unit="s",
            )
        )
        recovered.client.sock.settimeout(remaining(deadline, 6))
        devices_ok = recovered.recover_devices()
        host["checks"].append(
            dict(
                group="lifecycle",
                name="devices_after_reload",
                status="pass" if devices_ok else "fail",
                detail="fresh verified DMA transfer and bidirectional UART interrupts/data after overlay reload",
            )
        )
        host["elapsed_seconds"] = time.monotonic() - started
        host["checks"].append(
            dict(
                group="lifecycle",
                name="end_to_end_budget",
                status="pass" if host["elapsed_seconds"] < 60 else "fail",
                detail="reboot, onboard checks, SDK protocol and recovery all inside 60 seconds",
                value=host["elapsed_seconds"],
                unit="s",
            )
        )
        host["status"] = "fail" if any(c["status"] == "fail" for c in host["checks"]) else "pass"
        recovered.client.sock.settimeout(remaining(deadline))
        if not recovered.record_host_results(onboard["run_id"], json.dumps(host, allow_nan=False)):
            raise RuntimeError("Host results were not retained onboard")
        recovered.client.sock.settimeout(remaining(deadline))
        onboard = recovered.get_report()
        save(args.output, onboard, host)
        remaining(deadline)
    counts = {
        status: sum(c["status"] == status for c in onboard["checks"] + host["checks"])
        for status in ("pass", "fail", "skipped")
    }
    print(
        f"Full validation {host['status'].upper()} in {host['elapsed_seconds']:.3f} s: {counts}; {args.output / 'report.json'}"
    )
    if host["status"] != "pass":
        raise RuntimeError("Full validation failed; see the retained scorecard and report")
    return report_summary(onboard, host)


def report_summary(onboard, host):
    return dict(
        run_id=onboard["run_id"],
        status=host["status"],
        seconds=host["elapsed_seconds"],
        observers=onboard["observer_requests"],
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default=os.environ.get("HOST", "192.168.1.85"))
    parser.add_argument("--no-reboot", action="store_true")
    parser.add_argument("--analog-mask", type=lambda x: int(x, 0), choices=range(4), default=1)
    parser.add_argument("--seed", type=lambda x: int(x, 0), default=0xB157F011)
    parser.add_argument("--ssh-known-hosts", default=os.environ.get("BIST_SSH_KNOWN_HOSTS"))
    parser.add_argument("--output", type=Path, default=Path("tmp/bist-full-results"))
    args = parser.parse_args()
    try:
        validate(args)
    except (OSError, ValueError, RuntimeError, requests.RequestException) as error:
        parser.exit(1, f"bist-full: {error}\n")


if __name__ == "__main__":
    main()
