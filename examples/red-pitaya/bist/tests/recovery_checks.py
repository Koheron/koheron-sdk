#!/usr/bin/env python3
"""Hardware checks for process death and invalid report checkpoints.

Creates its own health run before touching that run's checkpoint. Requires
SSH access to the board; use the normal management API for overlay recovery.
"""
import argparse
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from client import api, connect, ssh


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="192.168.1.84")
    parser.add_argument("--ssh-known-hosts")
    parser.add_argument("--output", type=Path, default=Path("tmp/bist-recovery"))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    remote = ssh(args)

    def command(text):
        _, stdout, stderr = remote.exec_command(text, timeout=5)
        result = stdout.read().decode()
        assert stdout.channel.recv_exit_status() == 0, stderr.read().decode()
        return result

    def restart():
        api(args.host, "instruments/control/stop", True)
        api(args.host, "instruments/control/start", True)
        return connect(args.host, time.monotonic() + 10)

    def save(name, report):
        (args.output / f"{name}.json").write_text(json.dumps(report, indent=2) + "\n")

    client = connect(args.host, time.monotonic() + 10)
    assert client.start(1)
    run_id = client.get_report()["run_id"]
    deadline = time.monotonic() + 10
    while "Combined CPU/memory" not in client.get_log(0)["text"]:
        assert time.monotonic() < deadline
        time.sleep(.02)
    client.close()
    command("systemctl kill --signal=KILL koheron-server.service")
    client = restart()
    report = client.get_report()
    save("killed", report)
    assert report["run_id"] == run_id and report["state"] == "interrupted"
    assert report["functional_status"] == "incomplete" and len(report["metrics"]) >= 10
    assert client.record_lifecycle(.1, True)
    print("SIGKILL during combined workload and overlay recovery: PASS", flush=True)

    # A fresh completed run provides two consistent checkpoint generations.
    assert client.start(1)
    while client.running():
        assert time.monotonic() < deadline + 10
        time.sleep(.05)
    assert client.record_lifecycle(.1, True)
    report = client.get_report()
    prefix = f"/var/lib/koheron/bist/{report['run_id']}"
    client.close()
    api(args.host, "instruments/control/stop", True)
    transfer = remote.open_sftp()
    originals = {}
    for name in ("checkpoint", "checkpoint.prev"):
        with transfer.open(f"{prefix}/{name}", "rb") as file:
            originals[name] = file.read()
    try:
        with transfer.open(f"{prefix}/checkpoint", "wb") as file:
            file.write(originals["checkpoint"][:19])
        api(args.host, "instruments/control/start", True)
        client = connect(args.host, time.monotonic() + 10)
        recovered = client.get_report()
        save("truncated", recovered)
        assert recovered["run_id"] == report["run_id"] and recovered["state"] == "complete"
        assert recovered["metrics"] and not recovered["storage_ok"]
        client.close()
        api(args.host, "instruments/control/stop", True)
        for name in originals:
            with transfer.open(f"{prefix}/{name}", "wb") as file:
                file.write(b"invalid checkpoint")
        api(args.host, "instruments/control/start", True)
        client = connect(args.host, time.monotonic() + 10)
        corrupt = client.get_report()
        save("corrupt", corrupt)
        assert corrupt["state"] == "corrupt" and corrupt["metrics"] == []
        assert not corrupt["storage_ok"] and corrupt["functional_status"] == "unavailable"
        client.close()
        print("Truncated checkpoint fallback and corrupt-generation rejection: PASS", flush=True)
    finally:
        api(args.host, "instruments/control/stop", True)
        for name, data in originals.items():
            with transfer.open(f"{prefix}/{name}", "wb") as file:
                file.write(data)
        transfer.close()
        client = restart()
        restored = client.get_report()
        assert restored["metrics"] == report["metrics"] and restored["storage_ok"]
        client.close()
        remote.close()


if __name__ == "__main__":
    main()
