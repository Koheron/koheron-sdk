#!/usr/bin/env python3
"""BIST RPC client and bounded reboot/stop/start validation orchestrator."""
import argparse
import json
import math
import os
from pathlib import Path
import sys
import subprocess
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "python"))
from koheron import KoheronClient, command
import requests
sys.path.insert(0, str(Path(__file__).resolve().parent))
from performance import build_baseline, evaluate


class BistClient:
    def __init__(self, host, timeout=5):
        self.client = KoheronClient(host, timeout=timeout)
        try:
            self.client.get_ids("BistCompletion", "wait")
        except KeyError:
            self.close()
            raise RuntimeError("Quiet collection requires BIST 1.5 or newer; update the instrument") from None

    def close(self):
        self.client.sock.close()

    @command(classname="Bist")
    def start(self, analog_mask):
        return self.client.recv_bool()

    @command(classname="Bist")
    def start_profile(self, analog_mask, profile):
        return self.client.recv_bool()

    @command(classname="Bist")
    def start_comparison(self, case_mask):
        return self.client.recv_bool()

    @command(classname="Bist")
    def record_performance(self, run_id, status, comparison_json):
        return self.client.recv_bool()

    @command(classname="Bist")
    def cancel(self):
        pass

    @command(classname="Bist")
    def running(self):
        return self.client.recv_bool()

    @command(classname="BistCompletion")
    def wait(self, timeout_ms):
        return self.client.recv_bool()

    @command(classname="Bist")
    def get_boot_seconds(self):
        return self.client.recv_double()

    @command(classname="Bist")
    def get_boot_id(self):
        return self.client.recv_string()

    @command(classname="Bist")
    def get_report(self):
        return self.client.recv_json()

    @command(classname="Bist")
    def get_scorecard(self):
        return self.client.recv_string()

    @command(classname="Bist")
    def get_log(self, cursor):
        return self.client.recv_json()

    @command(classname="Bist")
    def ping(self, value):
        return self.client.recv_uint32()

    @command(classname="Bist")
    def record_startup(self, network_seconds, server_seconds, boot_seconds):
        return self.client.recv_bool()

    @command(classname="Bist")
    def record_lifecycle(self, restart_seconds, recovered):
        return self.client.recv_bool()


def api(host, route, post=False):
    response = requests.request("POST" if post else "GET", f"http://{host}/api/{route}", timeout=10)
    response.raise_for_status()
    return response.json()


def connect(host, deadline):
    while time.monotonic() < deadline:
        client = None
        accepted = False
        try:
            client = BistClient(host, timeout=min(5, deadline - time.monotonic()))
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            client.client.sock.settimeout(min(5, remaining))
            if client.ping(0x1234) == (0x1234 ^ 0x42495354):
                accepted = time.monotonic() < deadline
            if accepted:
                return client
        except (OSError, ValueError, KeyError, requests.RequestException):
            pass
        finally:
            if client is not None and not accepted:
                client.close()
        remaining = deadline - time.monotonic()
        if remaining > 0:
            time.sleep(min(.1, remaining))
    raise TimeoutError("BIST RPC readiness deadline exceeded")


def collect(client, deadline):
    """Sleep onboard until completion without polling the measured driver."""
    remaining = deadline - time.monotonic()
    if remaining > 0:
        sock = client.client.sock
        previous_timeout = sock.gettimeout()
        try:
            sock.settimeout(remaining)
            # Leave time for the timeout reply and a cancellation request.
            complete = client.wait(min(60000, max(0, int(remaining * 1000) - 50)))
        finally:
            sock.settimeout(previous_timeout)
        if complete:
            return client.get_report()
    client.cancel()
    raise TimeoutError("BIST suite deadline exceeded; partial report remains onboard")


def compare(reference, current):
    a, b = reference["metadata"], current["metadata"]
    reasons = []
    for key in ("fpga_sha256", "server_sha256", "bist_build_id", "workload_version", "parameters"):
        missing = False
        for label, metadata in (("reference", a), ("current", b)):
            value = metadata.get(key)
            if value is None or value == "unavailable" or not value:
                reasons.append(f"missing {label} {key}")
                missing = True
        if not missing and a[key] != b[key]:
            reasons.append(f"different {key}")
    old = {row["name"]: row for row in reference["metrics"]}
    rows = []
    for row in current["metrics"]:
        prior = old.pop(row["name"], None)
        if prior is None or prior["unit"] != row["unit"]:
            rows.append({"name": row["name"], "status": "missing or incompatible reference"})
        else:
            comparison = {"name": row["name"], "ratio": None,
                          "reference_pass": prior["pass"], "current_pass": row["pass"]}
            for label, measurement in (("reference", prior), ("current", row)):
                value = measurement.get("value")
                if type(value) not in (int, float) or not math.isfinite(value):
                    comparison["status"] = f"unavailable {label} value"
                    break
            else:
                if prior["value"]:
                    ratio = row["value"] / prior["value"]
                    if math.isfinite(ratio):
                        comparison["ratio"] = ratio
                    else:
                        comparison["status"] = "non-finite ratio"
                else:
                    comparison["status"] = "zero reference value"
            rows.append(comparison)
    rows.extend({"name": name, "status": "missing current measurement"} for name in old)
    return {"comparable": not reasons, "reasons": reasons, "metrics": rows,
            "reference_missing": reference.get("missing_measurements", []),
            "current_missing": current.get("missing_measurements", [])}


def ssh(args):
    import paramiko
    client = paramiko.SSHClient()
    if args.ssh_known_hosts:
        client.load_host_keys(args.ssh_known_hosts)
    else:
        client.load_system_host_keys()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(args.host, username="root", password=os.environ.get("BIST_SSH_PASSWORD", "changeme"),
                   allow_agent=True, look_for_keys=True, timeout=5)
    return client


def download(client, output):
    report = client.get_report()
    output.mkdir(parents=True, exist_ok=True)
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    (output / "scorecard.txt").write_text(client.get_scorecard())
    cursor = 0
    with (output / "log.txt").open("w") as log:
        while True:
            batch = client.get_log(cursor)
            log.write(batch["text"])
            if batch["cursor"] == cursor:
                break
            cursor = batch["cursor"]
    return report


def validate(args):
    started = time.monotonic()
    budget = 60 if getattr(args, "profile", "health") in ("benchmark", "comparison", "upload") else 30
    deadline = started + budget
    startup = None
    if not args.no_reboot:
        api(args.host, "instruments/default/bist", True)
        remote = ssh(args)
        _, stdout, _ = remote.exec_command("cat /proc/sys/kernel/random/boot_id", timeout=5)
        old_boot = stdout.read().decode().strip()
        reboot_started = time.monotonic()
        remote.exec_command("reboot")
        remote.close()
        # ICMP readiness is independent of nginx, SSH and the instrument server.
        # Require loss of the old boot before accepting the new echo reply.
        lost = False
        while time.monotonic() < deadline:
            reachable = subprocess.run(["ping", "-n", "-c", "1", "-W", "0.2", args.host],
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                       timeout=1).returncode == 0
            if reachable and lost:
                network = time.monotonic() - reboot_started
                break
            if not reachable:
                lost = True
            time.sleep(.1)
        else:
            raise TimeoutError("Network did not return after reboot")
        client = connect(args.host, deadline)
        server = time.monotonic() - reboot_started
        boot = client.get_boot_seconds()
        new_boot = client.get_boot_id()
        if not old_boot or not new_boot or new_boot == old_boot:
            raise RuntimeError("Reboot did not change the boot ID")
        startup = (network, server, float(boot))
    else:
        client = connect(args.host, deadline)
    profile = getattr(args, "profile", "health")
    accepted = client.start(args.analog_mask) if profile == "health" else client.start_profile(
        args.analog_mask, {"benchmark": 1, "diagnostic": 2, "comparison": 3, "upload": 4}[profile])
    if not accepted:
        raise RuntimeError("BIST start rejected (already active, invalid mask, or report storage unavailable)")
    collect(client, deadline)
    if startup and not client.record_startup(*startup):
        raise RuntimeError("Startup results were not stored")
    # Preserve the completed suite before a potentially failed service restart.
    retained = download(client, args.output)
    client.close()
    restart = time.monotonic()
    api(args.host, "instruments/control/stop", True)
    api(args.host, "instruments/control/start", True)
    client = connect(args.host, deadline)
    recovered = client.get_report()
    identical = recovered["run_id"] == retained["run_id"] and recovered["metrics"] == retained["metrics"]
    lifecycle_ok = client.record_lifecycle(time.monotonic() - restart, identical)
    report = download(client, args.output)
    if getattr(args, "save_baseline", None):
        references = [json.loads(path.read_text()) for path in args.reference_report] + [report]
        model = build_baseline(references, args.tolerance)
        args.save_baseline.write_text(json.dumps(model, indent=2, allow_nan=False) + "\n")
    performance = None
    if getattr(args, "baseline", None):
        performance = evaluate(json.loads(args.baseline.read_text()), report)
        if not client.record_performance(report["run_id"], performance["status"], json.dumps(performance, allow_nan=False)):
            raise RuntimeError("Performance comparison was not stored")
        report = download(client, args.output)
        (args.output / "performance.json").write_text(json.dumps(performance, indent=2) + "\n")
    client.close()
    if args.compare:
        comparison = compare(json.loads(args.compare.read_text()), report)
        (args.output / "comparison.json").write_text(json.dumps(comparison, indent=2) + "\n")
    elapsed = time.monotonic() - started
    failures = [row["name"] for row in report["metrics"] if not row["pass"]]
    print(f"Report ready in {elapsed:.3f} s: {args.output / 'report.json'}")
    if performance:
        print(f"Functional: {report['functional_status']}; performance: {performance['status']}")
        if performance.get("reason"):
            print(performance["reason"])
    if performance and performance["status"] == "regression":
        raise RuntimeError("Performance regression detected; functional results remain separate")
    if failures or not lifecycle_ok or not report["storage_ok"] or report["state"] != "complete" or elapsed >= budget:
        raise RuntimeError(f"Validation failed: {failures}, state={report['state']}, storage_ok={report['storage_ok']}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default=os.environ.get("HOST", "192.168.1.84"))
    parser.add_argument("--analog-mask", type=lambda value: int(value, 0), choices=range(4), default=1,
                        help="Connected DAC/ADC loopbacks: bit 0=channel 0, bit 1=channel 1")
    parser.add_argument("--no-reboot", action="store_true", help="Skip startup measurements")
    parser.add_argument("--ssh-known-hosts", help="SSH host-key file, checked on reboot connection")
    parser.add_argument("--output", type=Path, default=Path("tmp/bist-results"))
    parser.add_argument("--compare", type=Path, help="Previous runtime report for OS comparison")
    parser.add_argument("--profile", choices=("health", "benchmark", "diagnostic", "comparison", "upload"), default="health")
    parser.add_argument("--baseline", type=Path, help="Evaluate against a repeated-run baseline model")
    parser.add_argument("--save-baseline", type=Path, help="Save a baseline using this run and reference reports")
    parser.add_argument("--reference-report", type=Path, action="append", default=[])
    parser.add_argument("--tolerance", type=float, default=.10)
    args = parser.parse_args()
    try:
        validate(args)
    except (OSError, ValueError, RuntimeError, TimeoutError, requests.RequestException) as error:
        parser.exit(1, f"BIST: {error}\n")


if __name__ == "__main__":
    main()
