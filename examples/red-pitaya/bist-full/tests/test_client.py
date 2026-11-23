import importlib.util
from pathlib import Path
from unittest.mock import Mock, call

import pytest

spec = importlib.util.spec_from_file_location(
    "bist_full_client", Path(__file__).parents[1] / "client.py"
)
client = importlib.util.module_from_spec(spec)
spec.loader.exec_module(client)


def report():
    value = {
        "instrument": "bist-full",
        "state": "complete",
        "storage_ok": True,
        "completed_groups": sorted(client.GROUPS),
        "metadata": dict.fromkeys(
            ("source_sha256", "fpga_sha256", "server_sha256", "fpga_dna", "boot_id"), "identified"
        ),
        "checks": [
            {
                "group": g,
                "name": n,
                "status": "skipped" if (g, n) == ("analog", "channel_1") else "pass",
            }
            for g, n in sorted(client.required_checks(1))
        ],
    }

    value["metadata"]["parameters"] = {"analog_mask": 1}
    return value


def test_complete_report():
    assert client.validate_report(report()) == []


@pytest.mark.parametrize(
    "field,value",
    [
        ("state", "running"),
        ("state", "timeout"),
        ("storage_ok", False),
        ("completed_groups", []),
        ("checks", []),
        ("instrument", "bist"),
    ],
)
def test_partial_or_unhealthy_report_is_not_pass(field, value):
    value_report = report()
    value_report[field] = value
    assert client.validate_report(value_report)


def test_only_declared_external_analog_skip_is_accepted():
    value_report = report()
    row = next(r for r in value_report["checks"] if r["name"] == "channel_1")
    row["status"] = "skipped"
    assert client.validate_report(value_report) == []
    row["group"] = "uart"
    assert client.validate_report(value_report)


def test_connected_analog_cases_cannot_be_skipped():
    value_report = report()
    next(r for r in value_report["checks"] if r["name"].startswith("ch0_bin"))["status"] = "skipped"
    assert client.validate_report(value_report)


def test_completed_groups_do_not_hide_a_missing_case():
    value_report = report()
    value_report["checks"].pop()
    assert any(e.startswith("missing check:") for e in client.validate_report(value_report))


def test_duplicate_or_failed_check_is_not_accepted():
    value_report = report()
    value_report["checks"] *= 2
    assert client.validate_report(value_report)
    value_report = report()
    value_report["checks"][0]["status"] = "fail"
    assert client.validate_report(value_report)


@pytest.mark.parametrize(
    "identity", ["source_sha256", "fpga_sha256", "server_sha256", "fpga_dna", "boot_id"]
)
def test_report_requires_actual_identity(identity):
    value_report = report()
    value_report["metadata"][identity] = "unavailable"
    assert client.validate_report(value_report)


def test_quiet_collection_uses_only_completion_and_restores_timeout(monkeypatch):
    monkeypatch.setattr(client.time, "monotonic", lambda: 100)
    connection = Mock()
    connection.wait.return_value = True
    connection.client.sock.gettimeout.return_value = 3
    connection.get_report.return_value = report()
    assert client.collect(connection, 110) == report()
    assert connection.method_calls == [
        call.client.sock.gettimeout(),
        call.client.sock.settimeout(10),
        call.wait(9950),
        call.client.sock.settimeout(3),
        call.get_report(),
    ]


def test_timeout_cancels_without_downloading_incomplete_state(monkeypatch):
    monkeypatch.setattr(client.time, "monotonic", lambda: 100)
    connection = Mock()
    connection.wait.return_value = False
    with pytest.raises(TimeoutError):
        client.collect(connection, 101)
    connection.cancel.assert_called_once()
    connection.get_report.assert_not_called()


def test_deadline_is_a_hard_limit(monkeypatch):
    monkeypatch.setattr(client.time, "monotonic", lambda: 100)
    with pytest.raises(TimeoutError):
        client.remaining(100)
    assert client.remaining(100.25, 3) == 0.25


def test_protocol_deadline_stops_new_calls_and_keeps_completed_checks(monkeypatch):
    clock = [100.0]
    monkeypatch.setattr(client.time, "monotonic", lambda: clock[0])
    connection = Mock()
    connection.start.return_value = False

    def ping(value):
        clock[0] += 0.3
        return value ^ 0x42494631

    connection.ping.side_effect = ping
    checks = []
    with pytest.raises(TimeoutError):
        client.protocol_checks("board", connection, 101, checks)
    assert 0 < connection.ping.call_count < 5
    assert len(checks) > 3
    connection.echo_u64.assert_not_called()


@pytest.mark.parametrize("level", [0, 8191])
def test_constant_adc_evidence_is_preserved_without_nan(tmp_path, level):
    import json
    import time

    names = [f"ch0_capture_{i}.json" for i in range(21)]
    raw = dict(
        observe_channel=0,
        sample_rate_hz=62500000,
        adc0=[level] * 8192,
        adc1=[level] * 8192,
        dac=[0] * 1024,
    )
    connection = Mock()
    connection.get_artifact.side_effect = lambda run, name: (
        {"files": names} if name == "ch_manifest.json" else raw
    )
    onboard = dict(
        run_id="123",
        metadata={"parameters": {"analog_mask": 1}},
        checks=[
            dict(
                group="analog",
                name="ch0_spectrum_13",
                capture_artifact=names[0],
                frequency_hz=1586914.0625,
                amplitude=0,
                sndr_db=0,
                thd_dbc=0,
                sfdr_db=0,
                status="fail",
            )
        ],
    )
    outcome = client.download_artifacts(connection, onboard, tmp_path, time.monotonic() + 30)
    assert (
        outcome["status"] == "pass"
    )  # Independent analysis agrees; hardware verdict stays failed.
    comparison = json.loads((tmp_path / "spectral-crosscheck.json").read_text())
    assert comparison[0]["match"]
    assert onboard["checks"][0]["status"] == "fail"
    json.dumps(comparison, allow_nan=False)
