#!/usr/bin/env python3
"""Bounded cancellation, process-death and checkpoint-corruption checks.

Only this script's newly created bist-full run is modified. Its checkpoints
are restored in finally blocks. Does not erase OS data or touch other instruments.
"""

import argparse
from contextlib import closing
import json
import os
from pathlib import Path
import sys
import threading
import time

import paramiko

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from client import api, collect, connect, remaining, validate_report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="192.168.1.85")
    parser.add_argument("--ssh-known-hosts", required=True)
    parser.add_argument("--output", type=Path, default=Path("tmp/bist-full-recovery"))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    deadline = started + 60
    summary = {}

    def save(name, report):
        (args.output / f"{name}.json").write_text(json.dumps(report, indent=2) + "\n")

    def restart():
        api(args.host, "instruments/control/stop", deadline, True)
        api(args.host, "instruments/control/start", deadline, True)
        return connect(args.host, deadline)

    def await_stress(connection):
        while time.monotonic() < deadline:
            report = connection.get_report()
            if any(row["name"] == "capture_overflow_recovery" for row in report["checks"]):
                assert report["state"] == "running"
                return report["run_id"]
            time.sleep(0.01)
        raise TimeoutError("Stress phase did not start")

    def waiter(connection, result):
        try:
            connection.client.sock.settimeout(remaining(deadline, 30))
            result.append(connection.wait(int(remaining(deadline, 30) * 1000)))
        except OSError:
            result.append("disconnected")
        finally:
            connection.close()

    with closing(connect(args.host, deadline)) as connection:
        assert connection.start(1, 1000, 1)
        report = collect(connection, deadline)
        save("budget-timeout", report)
        assert report["state"] == "timeout" and validate_report(report)
        summary["budget_expiration"] = "pass"

        assert connection.start(1, 35000, 2)
        assert not connection.start(1, 35000, 3), "active start was not rejected"
        await_stress(connection)
        result = []
        threads = [
            threading.Thread(target=waiter, args=(connect(args.host, deadline), result))
            for _ in range(2)
        ]
        for thread in threads:
            thread.start()
        when = time.monotonic()
        connection.cancel()
        for thread in threads:
            thread.join(remaining(deadline, 3))
            assert not thread.is_alive()
        summary["cancel_seconds"] = time.monotonic() - when
        assert result == [True, True] and summary["cancel_seconds"] < 2, result
        report = connection.get_report()
        assert report["state"] == "cancelled"
        save("cancelled-with-waiters", report)

        assert connection.start(1, 35000, 3)
        run_id = await_stress(connection)
        result = []
        thread = threading.Thread(target=waiter, args=(connect(args.host, deadline), result))
        thread.start()
        when = time.monotonic()
        api(args.host, "instruments/control/stop", deadline, True)
        summary["active_stop_seconds"] = time.monotonic() - when
        assert summary["active_stop_seconds"] < 3
        thread.join(remaining(deadline, 3))
        assert not thread.is_alive()
    with closing(restart()) as connection:
        report = connection.get_report()
        save("active-shutdown", report)
        assert report["run_id"] == run_id and report["state"] == "cancelled"
        assert connection.recover_devices()

    with paramiko.SSHClient() as remote:
        remote.load_host_keys(args.ssh_known_hosts)
        remote.connect(
            args.host,
            username="root",
            password=os.environ.get("BIST_SSH_PASSWORD", "changeme"),
            allow_agent=True,
            look_for_keys=True,
            timeout=remaining(deadline),
        )
        with closing(connect(args.host, deadline)) as connection:
            assert connection.start(1, 35000, 4)
            run_id = await_stress(connection)
            _, stdout, stderr = remote.exec_command(
                "systemctl kill --signal=KILL koheron-server.service", timeout=remaining(deadline)
            )
            assert stdout.channel.recv_exit_status() == 0, stderr.read().decode()
        with closing(restart()) as connection:
            report = connection.get_report()
            save("killed", report)
            assert report["run_id"] == run_id and report["state"] == "interrupted"
            assert connection.recover_devices()
            summary["process_death_recovery"] = "pass"
            # Create a complete, owned checkpoint pair before injecting corruption.
            assert connection.start(1, 35000, 5)
            report = collect(connection, deadline)
            assert not validate_report(report), validate_report(report)
            assert connection.record_host_results(report["run_id"], '{"test":"checkpoint fixture"}')
            report = connection.get_report()
        prefix = f"/var/lib/koheron/bist-full/{report['run_id']}"
        api(args.host, "instruments/control/stop", deadline, True)
        with remote.open_sftp() as transfer:
            originals = {}
            for name in ("checkpoint", "checkpoint.prev"):
                with transfer.open(f"{prefix}/{name}", "rb") as file:
                    originals[name] = file.read()
            try:
                with transfer.open(f"{prefix}/checkpoint", "wb") as file:
                    file.write(originals["checkpoint"][:19])
                with closing(restart()) as connection:
                    fallback = connection.get_report()
                    save("truncated", fallback)
                    assert fallback["run_id"] == report["run_id"] and not fallback["storage_ok"]
                    assert fallback["checks"] == report["checks"]
                api(args.host, "instruments/control/stop", deadline, True)
                for name in originals:
                    with transfer.open(f"{prefix}/{name}", "wb") as file:
                        file.write(b"invalid checkpoint")
                with closing(restart()) as connection:
                    corrupt = connection.get_report()
                    save("corrupt", corrupt)
                    assert (
                        corrupt["state"] == "corrupt"
                        and not corrupt["storage_ok"]
                        and not corrupt["checks"]
                    )
                summary["checkpoint_fallback_and_corruption"] = "pass"
            finally:
                api(args.host, "instruments/control/stop", deadline, True)
                for name, contents in originals.items():
                    with transfer.open(f"{prefix}/{name}", "wb") as file:
                        file.write(contents)
        with closing(restart()) as connection:
            restored = connection.get_report()
            assert restored["storage_ok"] and restored["checks"] == report["checks"]
            assert restored["host_results_json"] == report["host_results_json"]
            assert connection.recover_devices()
    summary["elapsed_seconds"] = time.monotonic() - started
    assert summary["elapsed_seconds"] < 60
    save("summary", summary)
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
