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
            "metrics": [{"name": "dma_combined", "value": rate, "unit": "MB/s", "pass": True}],
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
