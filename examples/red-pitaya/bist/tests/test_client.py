import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location("bist_client", Path(__file__).parents[1] / "client.py")
client = importlib.util.module_from_spec(spec)
spec.loader.exec_module(client)


def report(digest="hash", parameters=None):
    return {"metadata": {"fpga_sha256": digest, "workload_version": 1, "parameters": parameters or {"analog_mask": 1}},
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
