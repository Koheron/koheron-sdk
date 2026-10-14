#!/usr/bin/env python3
"""Repeat live workloads, exercise cancellation, and verify failure diagnostics.

Requires the declared DAC0/ADC0 cable and an unconnected ADC1 for the negative
check. Use client.py for the separate reboot/readiness validation.
"""
import argparse
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from client import BistClient, api, connect


def finish(client, seconds=30):
    deadline = time.monotonic() + seconds
    latencies = []
    while client.running():
        started = time.monotonic()
        assert client.ping(0x1234) == (0x1234 ^ 0x42495354)
        latencies.append(time.monotonic() - started)
        if time.monotonic() >= deadline:
            client.cancel()
            raise TimeoutError("Live suite exceeded its deadline")
        time.sleep(.05)
    return client.get_report(), max(latencies, default=0)


def save(output, name, report):
    (output / f"{name}.json").write_text(json.dumps(report, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="192.168.1.84")
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--output", type=Path, default=Path("tmp/bist-hardware"))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    client = BistClient(args.host)
    summaries = []
    for index in range(args.runs):
        assert client.start(1)
        report, latency = finish(client)
        save(args.output, f"soak-{index+1}", report)
        failures = [row["name"] for row in report["metrics"] if not row["pass"]]
        assert not failures, failures
        assert report["state"] == "complete" and report["storage_ok"]
        assert latency < 1.5, latency
        summaries.append({"run_id": report["run_id"], "max_ping_seconds": latency})
        print(f"soak {index+1}: PASS, max responsive ping {latency:.6f} s", flush=True)

    # A deliberately false cable declaration must fail instrument health while
    # leaving OS checks and runtime reports available.
    assert client.start(3)
    report, _ = finish(client)
    save(args.output, "negative-loopback", report)
    failures = [row["name"] for row in report["metrics"] if not row["pass"]]
    assert failures == ["analog_1"], failures
    assert next(row for row in report["metrics"] if row["name"] == "analog_1")["status"] == "fail"
    print("missing channel-1 cable detection: PASS", flush=True)

    # Stop the service during combined transfers. The driver must join workers,
    # preserve partial results, and release its mappings before overlay reload.
    assert client.start(1)
    run_id = client.get_report()["run_id"]
    deadline = time.monotonic() + 15
    while "Combined CPU/memory" not in client.get_log(0)["text"]:
        assert time.monotonic() < deadline
        time.sleep(.05)
    client.close()
    started = time.monotonic()
    api(args.host, "instruments/control/stop", True)
    stopped_seconds = time.monotonic() - started
    assert stopped_seconds < 3, stopped_seconds
    api(args.host, "instruments/control/start", True)
    client = connect(args.host, time.monotonic() + 15)
    report = client.get_report()
    save(args.output, "cancelled", report)
    assert report["run_id"] == run_id
    assert report["state"] == "cancelled", report["state"]
    assert len(report["metrics"]) >= 10 and report["missing_measurements"]
    assert client.record_lifecycle(time.monotonic() - started, True)
    client.close()
    print(f"active-transfer shutdown/recovery: PASS, stop {stopped_seconds:.6f} s", flush=True)
    (args.output / "summary.json").write_text(json.dumps({"soak": summaries,
        "negative_loopback": "pass", "active_shutdown": "pass",
        "stop_seconds": stopped_seconds}, indent=2) + "\n")


if __name__ == "__main__":
    main()
