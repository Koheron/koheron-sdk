import copy
import importlib.util
import json
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest

sys.path.insert(0, str(Path(__file__).parents[1]))

spec = importlib.util.spec_from_file_location("compare_sdk", Path(__file__).parents[1] / "compare_sdk.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def report(server="a", value=100):
    metadata = {key: "same" for key in module.IDENTITY}
    metadata["parameters"] = {"profile": "comparison", "controlled": {"case_mask": 1}}
    metadata["server_sha256"] = server
    distribution = {"p50": value, "p95": value * 2, "p99": value * 3}
    return {"metadata": metadata, "state": "complete", "storage_ok": True,
            "functional_status": "pass", "missing_measurements": [],
            "metrics": [{"name": "rpc_controlled_idle", "pass": True, "comparison_quality": "valid",
                         "rpc": {"completed": 1200, "latency_us": distribution,
                                 "response_us": distribution, "release_lateness_us": distribution},
                         "memory": {"completed": 0}, "dma": {"completed": 0}}]}


def pairs(values):
    return [{"pair": i + 1, "baseline": report(), "candidate": report("b", value)}
            for i, value in enumerate(values)]


def test_sdk_identity_can_differ_but_probe_and_environment_must_match():
    a, b = report(), report("b")
    assert module.identity_reasons(a, b) == []
    for key in module.IDENTITY:
        bad = copy.deepcopy(b)
        bad["metadata"][key] = "different"
        assert f"different {key}" in module.identity_reasons(a, bad)
        del bad["metadata"][key]
        assert f"missing {key}" in module.identity_reasons(a, bad)


def test_one_pair_is_provisional_even_for_large_effect():
    result = module.summarize(pairs([50]))
    assert result["provisional"] and result["comparable"]
    assert all(row["status"] == "inconclusive" for row in result["metrics"])


@pytest.mark.parametrize("values,status", [([80] * 5, "improvement"), ([120] * 5, "regression"),
                                          ([100] * 5, "within_tolerance"),
                                          ([70, 80, 100, 120, 130], "inconclusive")])
def test_paired_intervals_and_repeatability(values, status):
    result = module.summarize(pairs(values))
    assert result == module.summarize(pairs(values))
    assert all(row["status"] == status for row in result["metrics"])


@pytest.mark.parametrize("change", ["observer", "rate", "failure", "missing", "server", "identity"])
def test_bad_pairs_cannot_produce_a_performance_verdict(change):
    runs = pairs([80] * 5)
    bad = runs[2]["candidate"]
    row = bad["metrics"][0]
    if change == "observer":
        row["comparison_quality"] = "inconclusive"
    elif change == "rate":
        row["rpc"]["completed"] = 1100
    elif change == "failure":
        bad["functional_status"] = "fail"
    elif change == "missing":
        bad["metrics"] = []
    elif change == "server":
        bad["metadata"]["server_sha256"] = "unexpected"
    else:
        bad["metadata"]["kernel"] = "new-kernel"
    result = module.summarize(runs)
    assert not result["comparable"] and result["reasons"]
    assert all(row["status"] == "inconclusive" for row in result["metrics"])


def test_restore_runs_after_measurement_failure_and_raw_results_survive(monkeypatch, tmp_path):
    archive = tmp_path / "bist.zip"
    archive.write_bytes(b"fake")
    monkeypatch.setattr(module, "package_identity", lambda path: {"fpga_sha256": "same"})
    saved = report()
    calls = []

    def measure(host, package, identity, output, mask):
        output.mkdir(parents=True)
        module.save(output / "report.json", saved)
        calls.append(package.parent.name)
        if len(calls) == 2:
            raise RuntimeError("connection lost")
        return saved

    monkeypatch.setattr(module, "measure", measure)
    restores = []
    monkeypatch.setattr(module, "ManagementClient", lambda *a, **k: SimpleNamespace(
        deploy=lambda path, **kw: restores.append(path)))
    args = SimpleNamespace(output=tmp_path / "results", baseline=archive, candidate=archive,
                           restore_package=None, pairs=5, tolerance=.05, host="test", case_mask=1)
    with pytest.raises(RuntimeError, match="connection lost"):
        module.run(args)
    assert len(restores) == 1
    assert json.loads((args.output / "pair-1/candidate/report.json").read_text()) == saved


def test_run_alternates_order_and_prints_after_first_pair(monkeypatch, tmp_path, capsys):
    archive = tmp_path / "bist.zip"
    archive.write_bytes(b"fake")
    monkeypatch.setattr(module, "package_identity", lambda path: {"fpga_sha256": "same"})
    calls = []

    def measure(host, package, identity, output, mask):
        if len(calls) == 2:
            assert "100.0 → 80.0" in capsys.readouterr().out
        label = package.parent.name
        calls.append(label)
        return report(label, 100 if label == "baseline" else 80)

    monkeypatch.setattr(module, "measure", measure)
    monkeypatch.setattr(module, "ManagementClient", lambda *a, **k: SimpleNamespace(deploy=lambda *a, **k: None))
    args = SimpleNamespace(output=tmp_path / "results", baseline=archive, candidate=archive,
                           restore_package=None, pairs=2, tolerance=.05, host="test", case_mask=1)
    module.run(args)
    assert calls == ["baseline", "candidate", "candidate", "baseline"]
    assert (args.output / "comparison.json").exists()


def test_missing_percentile_remains_visible_and_inconclusive():
    runs = pairs([80] * 5)
    for pair in runs:
        pair["candidate"]["metrics"][0]["rpc"]["latency_us"]["p99"] = None
    result = module.summarize(runs)
    row = next(row for row in result["metrics"] if row["name"].endswith("latency_us.p99"))
    assert row["status"] == "inconclusive" and row["pairs"] == 0


def test_histogram_resolution_can_prevent_a_borderline_verdict():
    runs = pairs([94] * 5)
    for pair in runs:
        for label in ("baseline", "candidate"):
            pair[label]["metrics"][0]["rpc"]["latency_us"].update(
                method="log_linear_histogram", relative_error_max=.0078125, minimum_bin_us=.25)
    result = module.summarize(runs)
    row = next(row for row in result["metrics"] if row["name"].endswith("latency_us.p50"))
    assert row["bootstrap_ratio_interval_95"][1] < .95
    assert row["ratio_interval_95"][1] > .95
    assert row["status"] == "inconclusive"


def test_histogram_bounds_cover_absolute_and_relative_bin_error():
    distribution = {"method": "log_linear_histogram", "relative_error_max": .0078125,
                    "minimum_bin_us": .25}
    assert module.quantile_bounds(1, distribution) == (.875, 1.125)
    low, high = module.quantile_bounds(1000, distribution)
    assert low < 993 and high > 1007


def test_wrong_live_binary_is_rejected_after_raw_report_is_saved(monkeypatch, tmp_path):
    saved = report("unexpected")
    class Connection:
        closed = False
        def start_comparison(self, mask):
            return True
        def close(self):
            self.closed = True
    connection = Connection()
    monkeypatch.setattr(module, "ManagementClient", lambda *a, **k: SimpleNamespace(deploy=lambda *a, **k: None))
    monkeypatch.setattr(module, "connect", lambda *a: connection)
    monkeypatch.setattr(module, "collect", lambda *a: saved)
    def download(client, output):
        module.save(output / "report.json", saved)
        return saved
    monkeypatch.setattr(module, "download", download)
    with pytest.raises(RuntimeError, match="server_sha256 does not match"):
        module.measure("test", tmp_path / "bist.zip", {"server_sha256": "intended"}, tmp_path, 1)
    assert connection.closed
    assert json.loads((tmp_path / "report.json").read_text()) == saved
