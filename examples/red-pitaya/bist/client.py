#!/usr/bin/env python3
"""BIST RPC client and bounded reboot/stop/start validation orchestrator."""
import argparse
import json
import os
from pathlib import Path
import sys
import subprocess
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "python"))
from koheron import KoheronClient, command
import requests


class BistClient:
    def __init__(self, host):
        self.client = KoheronClient(host, timeout=5)

    def close(self):
        self.client.sock.close()

    @command(classname="Bist")
    def start(self, analog_mask):
        return self.client.recv_bool()

    @command(classname="Bist")
    def cancel(self):
        pass

    @command(classname="Bist")
    def running(self):
        return self.client.recv_bool()

    @command(classname="Bist")
    def get_boot_seconds(self):
        return self.client.recv_double()

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
        try:
            client = BistClient(host)
            if client.ping(0x1234) == (0x1234 ^ 0x42495354):
                return client
            client.close()
        except (OSError, ValueError, KeyError, requests.RequestException):
            pass
        time.sleep(.1)
    raise TimeoutError("BIST RPC readiness deadline exceeded")


def collect(client, deadline):
    cursor = 0
    while time.monotonic() < deadline:
        batch = client.get_log(cursor)
        print(batch["text"], end="", flush=True)
        cursor = batch["cursor"]
        if not client.running():
            batch = client.get_log(cursor)
            print(batch["text"], end="", flush=True)
            return client.get_report()
        time.sleep(.2)
    client.cancel()
    raise TimeoutError("BIST suite deadline exceeded; partial report remains onboard")


def compare(reference, current):
    a, b = reference["metadata"], current["metadata"]
    reasons = []
    for key in ("fpga_sha256", "server_sha256", "bist_build_id", "workload_version", "parameters"):
        if a.get(key) != b.get(key):
            reasons.append(f"different {key}")
    if a.get("fpga_sha256") in (None, "unavailable") or b.get("fpga_sha256") in (None, "unavailable"):
        reasons.append("missing FPGA digest")
    old = {row["name"]: row for row in reference["metrics"]}
    rows = []
    for row in current["metrics"]:
        prior = old.pop(row["name"], None)
        if prior is None or prior["unit"] != row["unit"]:
            rows.append({"name": row["name"], "status": "missing or incompatible reference"})
        else:
            ratio = row["value"] / prior["value"] if prior["value"] else None
            rows.append({"name": row["name"], "ratio": ratio, "reference_pass": prior["pass"], "current_pass": row["pass"]})
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


def validate(args):
    started = time.monotonic()
    deadline = started + 180
    startup = None
    if not args.no_reboot:
        api(args.host, "instruments/default/bist", True)
        remote = ssh(args)
        _, stdout, _ = remote.exec_command("cat /proc/sys/kernel/random/boot_id")
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
        remote = ssh(args)
        _, stdout, _ = remote.exec_command("cat /proc/sys/kernel/random/boot_id")
        if stdout.read().decode().strip() == old_boot:
            raise RuntimeError("Reboot did not change the boot ID")
        remote.close()
        startup = (network, server, float(boot))
    else:
        client = connect(args.host, deadline)
    if not client.start(args.analog_mask):
        raise RuntimeError("BIST start rejected (already active, invalid mask, or report storage unavailable)")
    report = collect(client, deadline)
    if startup and not client.record_startup(*startup):
        raise RuntimeError("Startup results were not stored")
    retained = client.get_report()
    client.close()
    restart = time.monotonic()
    api(args.host, "instruments/control/stop", True)
    api(args.host, "instruments/control/start", True)
    client = connect(args.host, deadline)
    recovered = client.get_report()
    identical = recovered["run_id"] == retained["run_id"] and recovered["metrics"] == retained["metrics"]
    lifecycle_ok = client.record_lifecycle(time.monotonic() - restart, identical)
    report = client.get_report()
    elapsed = time.monotonic() - started
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    (args.output / "scorecard.txt").write_text(client.get_scorecard())
    cursor = 0
    with (args.output / "log.txt").open("w") as log:
        while True:
            batch = client.get_log(cursor)
            log.write(batch["text"])
            if batch["cursor"] == cursor:
                break
            cursor = batch["cursor"]
    client.close()
    if args.compare:
        comparison = compare(json.loads(args.compare.read_text()), report)
        (args.output / "comparison.json").write_text(json.dumps(comparison, indent=2) + "\n")
    failures = [row["name"] for row in report["metrics"] if not row["pass"]]
    print(f"Report ready in {elapsed:.3f} s: {args.output / 'report.json'}")
    if failures or not lifecycle_ok or not report["storage_ok"] or report["state"] != "complete" or elapsed >= 180:
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
    args = parser.parse_args()
    try:
        validate(args)
    except (OSError, ValueError, RuntimeError, TimeoutError, requests.RequestException) as error:
        parser.exit(1, f"BIST: {error}\n")


if __name__ == "__main__":
    main()
