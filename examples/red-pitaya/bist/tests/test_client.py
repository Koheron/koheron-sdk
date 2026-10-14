import importlib.util
import json
import socket
import threading
import time
from unittest.mock import Mock
from types import SimpleNamespace
from pathlib import Path
import pytest

spec = importlib.util.spec_from_file_location("bist_client", Path(__file__).parents[1] / "client.py")
client = importlib.util.module_from_spec(spec)
spec.loader.exec_module(client)


def report(digest="hash", parameters=None):
    return {"metadata": {"fpga_sha256": digest, "server_sha256": "server-hash",
                         "bist_build_id": "source-hash", "workload_version": 1,
                         "parameters": parameters or {"analog_mask": 1}},
            "metrics": [{"name": "rate", "value": 10, "unit": "MB/s", "pass": True}]}


def test_comparison():
    old, new = report(), report()
    new["metrics"][0]["value"] = 20
    result = client.compare(old, new)
    assert result["comparable"]
    assert result["metrics"][0]["ratio"] == 2


def test_comparison_rejects_workload_and_fpga_changes():
    assert not client.compare(report(), report("new"))["comparable"]
    assert not client.compare(report(), report(parameters={"analog_mask": 3}))["comparable"]
    assert not client.compare(report("unavailable"), report("unavailable"))["comparable"]


def test_missing_measurement():
    new = report()
    new["metrics"] = []
    assert client.compare(report(), new)["metrics"] == [{"name": "rate", "status": "missing current measurement"}]


@pytest.mark.parametrize("key", ["fpga_sha256", "server_sha256", "bist_build_id",
                                 "workload_version", "parameters"])
@pytest.mark.parametrize("value", [None, "", "unavailable"])
def test_matching_missing_identities_are_not_comparable(key, value):
    old, new = report(), report()
    old["metadata"][key] = new["metadata"][key] = value
    result = client.compare(old, new)
    assert not result["comparable"]
    assert f"missing reference {key}" in result["reasons"]
    assert f"missing current {key}" in result["reasons"]


@pytest.mark.parametrize("key", ["fpga_sha256", "server_sha256", "bist_build_id",
                                 "workload_version", "parameters"])
@pytest.mark.parametrize("missing_report", ["reference", "current", "both"])
def test_omitted_identity_is_reported(key, missing_report):
    old, new = report(), report()
    if missing_report in ("reference", "both"):
        del old["metadata"][key]
    if missing_report in ("current", "both"):
        del new["metadata"][key]
    result = client.compare(old, new)
    assert not result["comparable"]
    assert any(key in reason for reason in result["reasons"])


@pytest.mark.parametrize("side", ["reference", "current"])
@pytest.mark.parametrize("value", [None, float("nan"), float("inf"), "unavailable", True])
def test_unavailable_measurement_has_no_ratio(side, value):
    old, new = report(), report()
    measurement = (old if side == "reference" else new)["metrics"][0]
    measurement.update(value=value, **{"pass": False})
    result = client.compare(old, new)
    assert result["comparable"]
    row = result["metrics"][0]
    assert row["ratio"] is None
    assert row["status"] == f"unavailable {side} value"
    assert row[f"{side}_pass"] is False
    json.dumps(result, allow_nan=False)


def test_zero_reference_does_not_divide():
    old = report()
    old["metrics"][0]["value"] = 0
    row = client.compare(old, report())["metrics"][0]
    assert row["ratio"] is None
    assert row["status"] == "zero reference value"


def test_non_finite_ratio_is_not_written_to_json():
    old, new = report(), report()
    old["metrics"][0]["value"] = 1e-308
    new["metrics"][0]["value"] = 1e308
    result = client.compare(old, new)
    assert result["metrics"][0]["ratio"] is None
    assert result["metrics"][0]["status"] == "non-finite ratio"
    json.dumps(result, allow_nan=False)


def test_readiness_closes_failed_ping_connections(monkeypatch):
    connection = Mock()
    connection.ping.side_effect = OSError("Disconnected during ping")
    monkeypatch.setattr(client, "BistClient", lambda *args, **kwargs: connection)
    with pytest.raises(TimeoutError):
        client.connect("test", time.monotonic() + .02)
    connection.close.assert_called_once()


def test_readiness_handshake_uses_remaining_deadline(monkeypatch):
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        listener.settimeout(1)
        stopped = threading.Event()

        def server():
            try:
                connection, _ = listener.accept()
            except OSError:
                return
            with connection:
                connection.recv(8)
                stopped.wait(1)

        def connect_socket(host, timeout=5):
            return client.KoheronClient(host, listener.getsockname()[1], timeout=timeout)

        monkeypatch.setattr(client, "BistClient", connect_socket)
        worker = threading.Thread(target=server)
        worker.start()
        started = time.monotonic()
        try:
            with pytest.raises(TimeoutError):
                client.connect("127.0.0.1", started + .05)
            assert time.monotonic() - started < .3
        finally:
            stopped.set()
            worker.join(timeout=1)
            assert not worker.is_alive()


def test_restart_failure_preserves_completed_suite_download(monkeypatch, tmp_path):
    saved = report()
    saved.update(run_id="123", state="complete", storage_ok=True)
    connection = Mock()
    connection.start.return_value = True
    connection.get_report.return_value = saved
    connection.get_scorecard.return_value = "Scorecard\n"
    connection.get_log.side_effect = [
        {"text": "Completed\n", "cursor": 10}, {"text": "", "cursor": 10}]
    monkeypatch.setattr(client, "connect", lambda *args: connection)
    monkeypatch.setattr(client, "collect", lambda *args: saved)

    def failed_stop(*args):
        raise client.requests.ConnectionError("Management connection lost during stop")

    monkeypatch.setattr(client, "api", failed_stop)
    args = SimpleNamespace(no_reboot=True, host="test", analog_mask=1,
                           output=tmp_path, compare=None)
    with pytest.raises(client.requests.ConnectionError):
        client.validate(args)
    assert json.loads((tmp_path / "report.json").read_text()) == saved
    assert (tmp_path / "scorecard.txt").read_text() == "Scorecard\n"
    assert (tmp_path / "log.txt").read_text() == "Completed\n"
    connection.close.assert_called_once()
