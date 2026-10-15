import copy
import importlib.util
from pathlib import Path
import pytest

spec = importlib.util.spec_from_file_location("bist_performance", Path(__file__).parents[1] / "performance.py")
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)


def report(run=0, rate=100):
    return {"run_id": str(run), "state": "complete", "storage_ok": True, "functional_status": "pass",
            "metadata": {key: {"profile": "health"} if key == "parameters" else "id" for key in p.IDENTITY},
            "metrics": [{"name": "dma_combined", "value": rate, "unit": "MB/s", "pass": True},
                        {"name": "combined_window", "observer_requests": 0, "pass": True}],
            "missing_measurements": []}


def baseline(rates=(99, 100, 101, 100, 99)):
    return p.build_baseline([report(i, rate) for i, rate in enumerate(rates)])


def test_known_regression_and_tolerance():
    model = baseline()
    assert p.evaluate(model, report(10, 99))["status"] == "pass"
    assert p.evaluate(model, report(10, 89))["status"] == "regression"
    assert p.evaluate(model, report(10, 150))["status"] == "pass"
    slower = report(10)
    slower["metrics"][0].update(name="register_read", unit="ns/read")
    reports = [copy.deepcopy(slower) for _ in range(5)]
    for i, item in enumerate(reports):
        item["run_id"] = str(i)
    model = p.build_baseline(reports)
    slower["metrics"][0]["value"] = 111
    assert p.evaluate(model, slower)["status"] == "regression"


def test_noisy_insufficient_and_missing_are_not_pass():
    assert p.evaluate(baseline((50, 75, 100, 125, 150)), report(10))["status"] == "inconclusive"
    assert p.evaluate(baseline((100, 100, 100, 100)), report(10))["status"] == "inconclusive"
    current = report(10)
    current["metrics"] = []
    assert p.evaluate(baseline(), current)["status"] == "inconclusive"


@pytest.mark.parametrize("field,value", [("state", "interrupted"), ("storage_ok", False),
                                         ("functional_status", "fail"),
                                         ("missing_measurements", ["adc_capture"])])
def test_failed_partial_reports_cannot_seed_baseline(field, value):
    bad = report()
    bad[field] = value
    with pytest.raises(ValueError):
        p.build_baseline([bad])
    assert p.evaluate(baseline(), bad)["status"] == "inconclusive"


def test_identity_mismatch_and_duplicate_runs():
    bad = report(10)
    bad["metadata"]["parameters"] = {"profile": "benchmark"}
    assert p.evaluate(baseline(), bad)["status"] == "incompatible"
    with pytest.raises(ValueError):
        p.build_baseline([report(), bad])
    with pytest.raises(ValueError):
        p.build_baseline([report(), report()])


def test_unavailable_tails_do_not_seed_samples():
    reports = [report(i) for i in range(5)]
    for r in reports:
        r["metrics"][0]["latency_ms"] = {"count": 5, "p50": 4, "p95": None, "p99": None}
    model = p.build_baseline(reports)
    assert {row["name"] for row in model["metrics"]} == {"dma_combined", "dma_combined.p50"}


def test_saturation_latency_is_descriptive_but_isolated_latency_is_compared():
    reports = [report(i) for i in range(5)]
    for r in reports:
        r["metrics"][0].update(name="rpc_combined", unit="requests/s", latency_us={"p50": 378})
    model = p.build_baseline(reports)
    current = copy.deepcopy(reports[0])
    current["metrics"][0]["latency_us"]["p50"] = 540
    assert p.evaluate(model, current)["status"] == "pass"
    assert {row["name"] for row in model["metrics"]} == {"rpc_combined"}
    for r in reports:
        r["metrics"][0]["name"] = "rpc_isolated"
    current["metrics"][0]["name"] = "rpc_isolated"
    assert p.evaluate(p.build_baseline(reports), current)["status"] == "regression"


@pytest.mark.parametrize("value", [None, "", "unavailable", {}, 0, False])
def test_missing_baseline_identity_is_rejected(value):
    bad = report()
    bad["metadata"]["parameters"] = value
    with pytest.raises(ValueError):
        p.build_baseline([bad])


def test_batch_resolution_prevents_false_dma_regression():
    reports = [report(i, 5.24288) for i in range(5)]
    for r in reports:
        r["metadata"]["parameters"].update(combined_window_seconds=2, dma_bytes=2097152)
    model = p.build_baseline(reports)
    current = copy.deepcopy(reports[0])
    current["metrics"][0]["value"] = 4.194304
    assert p.evaluate(model, current)["status"] == "pass"
    current["metrics"][0]["value"] = 3.145728
    assert p.evaluate(model, current)["status"] == "regression"
    assert model["metrics"][0]["resolution"] == pytest.approx(1.048576)


@pytest.mark.parametrize("count", [1, 15, -1, None, True, "0", 0.0])
def test_disturbed_or_unverified_measurements_cannot_seed_baseline(count):
    bad = report(10)
    bad["metrics"][1]["observer_requests"] = count
    with pytest.raises(ValueError, match="observer"):
        p.build_baseline([bad])
    result = p.evaluate(baseline(), bad)
    assert result["status"] == "inconclusive"
    assert "observer" in result["reason"]
    assert bad["functional_status"] == "pass"


def test_missing_observer_counts_are_not_assumed_quiet():
    bad = report(10)
    del bad["metrics"][1]["observer_requests"]
    with pytest.raises(ValueError, match="observer"):
        p.build_baseline([bad])
    bad["metrics"].pop()
    assert p.evaluate(baseline(), bad)["status"] == "inconclusive"


def test_old_baseline_requires_recollection():
    model = baseline()
    assert model["schema_version"] == 2
    assert all(row["observer_requests"] == 0 for row in model["conditions"])
    model["schema_version"] = 1
    result = p.evaluate(model, report(10))
    assert result["status"] == "inconclusive"
    assert "rebuild" in result["reason"]
