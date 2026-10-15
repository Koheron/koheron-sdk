"""Repeatable performance comparisons, independent of functional health."""
import math
from statistics import median

IDENTITY = ("fpga_sha256", "server_sha256", "bist_build_id", "workload_version", "parameters",
            "board", "fpga_dna")
DIRECTIONS = {"register_read": "lower", "register_write": "lower", "bram_write": "higher",
              "bram_read": "higher", "dma_isolated": "higher", "dma_combined": "higher",
              "uart_isolated": "higher", "uart_combined": "higher",
              "uart_roundtrip_isolated": "lower", "uart_roundtrip_combined": "lower",
              "uio_observation": "lower", "rpc_isolated": "higher", "rpc_bulk": "higher",
              "rpc_combined": "higher", "cpu_memory_load": "higher"}


def finite(value):
    return type(value) in (int, float) and math.isfinite(value)


def identity(metadata):
    result = {}
    for key in IDENTITY:
        value = metadata.get(key)
        if not value or value == "unavailable":
            raise ValueError(f"Missing baseline identity: {key}")
        result[key] = value
    if not isinstance(result["parameters"], dict):
        raise ValueError("Baseline parameters must be an object")
    return result


def healthy(report):
    if report.get("state") != "complete" or not report.get("storage_ok"):
        raise ValueError("Baseline requires complete reports with healthy storage")
    if report.get("functional_status") != "pass" or any(not row["pass"] for row in report["metrics"]):
        raise ValueError("Baseline requires functional PASS")
    missing = set(report.get("missing_measurements", [])) - {
        "network_readiness", "server_readiness", "boot_readiness"}
    if missing:
        raise ValueError(f"Baseline has missing measurements: {sorted(missing)}")
    observer_requests(report)


def observer_requests(report):
    windows = [row for row in report["metrics"] if row["name"] == "combined_window"]
    if not windows:
        raise ValueError("Performance comparison requires combined-window observer counts")
    for row in windows:
        count = row.get("observer_requests")
        if type(count) is not int or count < 0:
            raise ValueError("Performance comparison requires valid observer counts")
        if count:
            raise ValueError(f"Measurement disturbed by {count} observer requests; rerun without polling")
    return 0


def measurements(report):
    result = {}
    for row in report["metrics"]:
        if row["name"] not in DIRECTIONS:
            continue
        if finite(row.get("value")):
            result[row["name"]] = (row["value"], row["unit"], DIRECTIONS[row["name"]])
        # Saturation RPC latency changes with the other workers' achieved load.
        # Keep its capacity rate; use the controlled profile for latency verdicts.
        if row["name"] == "rpc_combined":
            continue
        for field, unit in (("latency_us", "us"), ("latency_ms", "ms")):
            distribution = row.get(field)
            if not isinstance(distribution, dict):
                continue
            for percentile in ("p50", "p95", "p99"):
                value = distribution.get(percentile)
                if finite(value):
                    result[f"{row['name']}.{percentile}"] = (value, unit, "lower")
    return result


def build_baseline(reports, tolerance=.10):
    if not 0 < tolerance < 1 or not reports:
        raise ValueError("Provide reports and a tolerance between zero and one")
    expected = identity(reports[0]["metadata"])
    samples = {}
    seen = set()
    for report in reports:
        healthy(report)
        if identity(report["metadata"]) != expected:
            raise ValueError("Baseline reports have different reference identities or parameters")
        run_id = report.get("run_id")
        if not run_id or run_id in seen:
            raise ValueError("Baseline requires distinct run IDs")
        seen.add(run_id)
        for name, sample in measurements(report).items():
            samples.setdefault(name, []).append(sample)
    rows = []
    for name, values in samples.items():
        units = {item[1] for item in values}
        if len(units) != 1:
            raise ValueError(f"Different units for {name}")
        center = median(item[0] for item in values)
        mad = median(abs(item[0] - center) for item in values)
        noise = 3 * 1.4826 * mad
        parameters = expected["parameters"]
        window = parameters.get("combined_window_seconds", 0)
        resolution = 0
        if finite(window) and window > 0:
            if name == "dma_combined":
                resolution = parameters.get("dma_bytes", 0) / window / 1e6
            elif name == "cpu_memory_load":
                resolution = 2 * 8 * 1024 * 1024 / window / 1e6
        status = "ready"
        if len(values) < 5 or center <= 0 or resolution >= center:
            status = "insufficient"
        elif noise > center * tolerance:
            status = "noisy"
        rows.append({"name": name, "unit": values[0][1], "direction": values[0][2],
                     "count": len(values), "median": center, "mad": mad,
                     "margin": max(center * tolerance, noise, resolution),
                     "resolution": resolution, "status": status})
    return {"schema_version": 2, "identity": expected, "tolerance": tolerance,
            "minimum_runs": 5, "runs": sorted(seen), "metrics": rows,
            "conditions": [{"run_id": r["run_id"], "os": r["metadata"].get("os"),
                            "kernel": r["metadata"].get("kernel"),
                            "observer_requests": observer_requests(r),
                            "environment": r["metadata"].get("environment")} for r in reports]}


def evaluate(baseline, report):
    if baseline.get("schema_version") == 1:
        return {"status": "inconclusive", "reason": "Baseline predates observer validation; rebuild it from quiet runs", "metrics": []}
    if baseline.get("schema_version") != 2:
        raise ValueError("Unsupported baseline schema")
    if identity(report["metadata"]) != baseline["identity"]:
        return {"status": "incompatible", "reason": "Different reference identities or parameters", "metrics": []}
    try:
        healthy(report)
    except ValueError as error:
        return {"status": "inconclusive", "reason": str(error), "metrics": []}
    current = measurements(report)
    rows = []
    for reference in baseline["metrics"]:
        row = {"name": reference["name"], "status": reference["status"]}
        measurement = current.get(reference["name"])
        if measurement and measurement[1] == reference["unit"]:
            row.update(value=measurement[0], unit=measurement[1], median=reference["median"],
                       resolution=reference.get("resolution", 0))
        if reference["status"] not in ("ready", "noisy", "insufficient"):
            raise ValueError("Invalid baseline metric status")
        if reference["status"] == "ready" and (reference["count"] < 5 or len(set(baseline["runs"])) < 5):
            row["status"] = "insufficient"
        elif not measurement or measurement[1] != reference["unit"]:
            row["status"] = "unavailable"
        elif reference["status"] == "ready":
            value = measurement[0]
            center, margin = reference["median"], reference["margin"]
            if not finite(center) or not finite(margin) or center <= 0 or margin < 0:
                raise ValueError("Invalid baseline limits")
            if reference["direction"] not in ("higher", "lower"):
                raise ValueError("Invalid baseline direction")
            threshold = center - margin if reference["direction"] == "higher" else center + margin
            regressed = (value < threshold if reference["direction"] == "higher" else value > threshold)
            regressed = regressed and not math.isclose(value, threshold, rel_tol=1e-12)
            row.update(status="regression" if regressed else "pass", value=value,
                       median=center, threshold=threshold, unit=reference["unit"])
        rows.append(row)
    status = "regression" if any(r["status"] == "regression" for r in rows) else \
             "pass" if rows and all(r["status"] == "pass" for r in rows) else "inconclusive"
    return {"status": status, "baseline_runs": baseline["runs"], "metrics": rows}
