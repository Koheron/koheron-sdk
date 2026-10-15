#!/usr/bin/env python3
"""Alternate two SDK packages under identical, controlled BIST workloads."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import random
import shutil
import statistics
import subprocess
import sys
import time
import zipfile

from client import collect, connect, download
from koheron.management import Client as ManagementClient, ManagementError

ROOT = Path(__file__).resolve().parents[3]
CFG = "examples/red-pitaya/bist/stemlab-125-14.mk"
ARTIFACTS = Path("tmp/examples/red-pitaya/bist/stemlab-125-14")
CASES = ("idle", "memory", "dma", "mixed", "upload_value", "upload_reference")
IDENTITY = ("bist_build_id", "workload_version", "parameters", "board", "fpga_dna",
            "fpga_sha256", "kernel", "os", "os_image", "os_manifest", "boot_id", "kernel_cmdline")


def save(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n")


def package_identity(path):
    try:
        with zipfile.ZipFile(path) as archive:
            if not archive.read("version").decode().strip():
                raise ValueError("Empty instrument package version")
            manifest = json.loads(archive.read("instrument.json"))
            if not isinstance(manifest, dict):
                raise ValueError("Instrument manifest must be an object")
            return {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                    "server_sha256": hashlib.sha256(archive.read("serverd")).hexdigest(),
                    "fpga_sha256": hashlib.sha256(archive.read("bist.bit.bin")).hexdigest(),
                    "manifest": manifest}
    except (zipfile.BadZipFile, KeyError, UnicodeError) as error:
        raise ValueError(f"Invalid BIST archive {path}: {error}") from error


def build(ref, output, fpga_cache):
    """Overlay this exact probe on either SDK; reuse only immutable FPGA outputs."""
    commit = subprocess.check_output(["git", "rev-parse", "--verify", "--end-of-options", ref + "^{commit}"],
                                     cwd=ROOT, text=True).strip()
    checkout = output / "checkout"
    subprocess.run(["git", "worktree", "add", "--detach", str(checkout), commit], cwd=ROOT, check=True,
                   stdout=subprocess.DEVNULL)
    try:
        probe = checkout / "examples/red-pitaya/bist"
        if probe.exists():
            shutil.rmtree(probe)
        shutil.copytree(Path(__file__).parent, probe, ignore=shutil.ignore_patterns("__pycache__", ".pytest_cache"))
        cache = checkout / ARTIFACTS
        cache.mkdir(parents=True)
        immutable = ("bist.bit", "bist.bit.bin", "pl.dtbo", "fpga/bist.xsa")
        for name in immutable:
            target = cache / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(fpga_cache / name, target)
        command = ["make", str(ARTIFACTS / "bist.zip"), "CFG=" + CFG,
                   "VENV=" + str(Path(sys.executable).parent.parent), "-j4", "N_CPUS=4", "V=0"]
        for name in immutable:
            command.extend(("-o", str(ARTIFACTS / name)))
        print(f"Building {ref} ({commit[:12]}); log: {output / 'build.log'}", flush=True)
        with (output / "build.log").open("w") as log:
            subprocess.run(command, cwd=checkout, stdout=log, stderr=subprocess.STDOUT, check=True)
        archive = output / "bist.zip"
        shutil.copy2(cache / "bist.zip", archive)
        save(output / "source.json", {"ref": ref, "commit": commit, "probe_source": str(Path(__file__).parent),
                                      "command": command})
        return archive
    finally:
        subprocess.run(["git", "worktree", "remove", "--force", str(checkout)], cwd=ROOT, check=True,
                       stdout=subprocess.DEVNULL)


def identity_reasons(a, b):
    reasons = []
    for key in IDENTITY:
        left, right = a.get("metadata", {}).get(key), b.get("metadata", {}).get(key)
        if left in (None, "", "unavailable", {}) or right in (None, "", "unavailable", {}):
            reasons.append(f"missing {key}")
        elif left != right:
            reasons.append(f"different {key}")
    # Server binary and SDK commit may differ, but both must be identified.
    for key in ("cpu0_governor",):
        if a.get("metadata", {}).get("environment", {}).get(key) != b.get("metadata", {}).get("environment", {}).get(key):
            reasons.append(f"different {key}")
    for report in (a, b):
        if report.get("metadata", {}).get("server_sha256") in (None, "", "unavailable"):
            reasons.append("missing server_sha256")
        if report.get("state") != "complete" or not report.get("storage_ok") or report.get("functional_status") != "pass":
            reasons.append("incomplete, unhealthy, or failed run")
        if report.get("missing_measurements"):
            reasons.append("missing measurements")
    return sorted(set(reasons))


def case_reasons(a, b):
    reasons = []
    for row in (a, b):
        if not row.get("pass") or row.get("comparison_quality") != "valid":
            reasons.append("invalid controlled load or integrity")
    for worker in ("rpc", "memory", "dma"):
        x, y = a.get(worker, {}).get("completed", 0), b.get(worker, {}).get("completed", 0)
        if max(x, y) and abs(x - y) / max(x, y) > .02:
            reasons.append(f"different achieved {worker} load (>2%)")
    return reasons


def interval(ratios):
    """Deterministic paired bootstrap of the median candidate/reference ratio."""
    if min(ratios) == max(ratios):
        return [ratios[0], ratios[0]]
    rng = random.Random(0)
    samples = sorted(statistics.median(rng.choices(ratios, k=len(ratios))) for _ in range(10000))
    return [samples[249], samples[9749]]


def quantile_bounds(value, distribution):
    """Conservative bounds for the histogram estimate, including sub-16 us bins."""
    if distribution.get("method") != "log_linear_histogram":
        return value, value
    relative = distribution["relative_error_max"]
    absolute = distribution["minimum_bin_us"] / 2
    if not (math.isfinite(relative) and 0 <= relative < 1 and math.isfinite(absolute) and absolute >= 0):
        raise ValueError("Invalid histogram resolution")
    return max(0, min(value / (1 + relative), value - absolute)), max(value / (1 - relative), value + absolute)


def summarize(pairs, tolerance=.05):
    reasons, series, uncertainty, unavailable = [], {}, {}, set()
    anchor = pairs[0]["baseline"] if pairs else None
    expected_servers = {}
    for pair in pairs:
        a, b = pair["baseline"], pair["candidate"]
        reasons.extend(identity_reasons(a, b))
        reasons.extend(identity_reasons(anchor, a))
        reasons.extend(identity_reasons(anchor, b))
        for label, report in (("baseline", a), ("candidate", b)):
            server = report["metadata"].get("server_sha256")
            if label in expected_servers and expected_servers[label] != server:
                reasons.append(f"different {label} server between runs")
            expected_servers[label] = server
        old = {row["name"]: row for row in a["metrics"] if row["name"].startswith("rpc_controlled_")}
        new = {row["name"]: row for row in b["metrics"] if row["name"].startswith("rpc_controlled_")}
        if old.keys() != new.keys() or not old:
            reasons.append("different or missing controlled cases")
        for name in old.keys() & new.keys():
            invalid = case_reasons(old[name], new[name])
            reasons.extend(f"pair {pair['pair']} {name}: {reason}" for reason in invalid)
            for distribution in ("latency_us", "response_us", "release_lateness_us"):
                for percentile in ("p50", "p95", "p99"):
                    key = f"{name}.{distribution}.{percentile}"
                    x = (old[name].get("rpc", {}).get(distribution) or {}).get(percentile)
                    y = (new[name].get("rpc", {}).get(distribution) or {}).get(percentile)
                    if not all(type(v) in (int, float) and math.isfinite(v) and v > 0 for v in (x, y)):
                        unavailable.add(key)
                        continue
                    low_x, high_x = quantile_bounds(x, old[name]["rpc"][distribution])
                    low_y, high_y = quantile_bounds(y, new[name]["rpc"][distribution])
                    if low_x <= 0:
                        unavailable.add(key)
                        continue
                    series.setdefault(key, []).append((x, y))
                    uncertainty.setdefault(key, []).append((low_y / high_x, high_y / low_x))
    rows = []
    for name in sorted(series.keys() | unavailable):
        values = series.get(name, [])
        if not values:
            rows.append({"name": name, "pairs": 0, "status": "inconclusive",
                         "reason": "missing or insufficient percentile samples"})
            continue
        ratios = [y / x for x, y in values]
        bootstrap = interval(ratios)
        bounds = [interval([low for low, _ in uncertainty[name]])[0],
                  interval([high for _, high in uncertainty[name]])[1]]
        status = "inconclusive"
        if not reasons and len(values) == len(pairs) and len(values) >= 5 and name not in unavailable:
            if bounds[1] < 1 - tolerance:
                status = "improvement"
            elif bounds[0] > 1 + tolerance:
                status = "regression"
            elif bounds[0] >= 1 - tolerance and bounds[1] <= 1 + tolerance:
                status = "within_tolerance"
        rows.append({"name": name, "baseline_median_us": statistics.median(x for x, _ in values),
                     "candidate_median_us": statistics.median(y for _, y in values),
                     "paired_ratio": statistics.median(ratios), "bootstrap_ratio_interval_95": bootstrap,
                     "ratio_interval_95": bounds,
                     "pairs": len(values), "status": status})
    return {"schema_version": 1, "pairs": len(pairs), "provisional": len(pairs) < 5,
            "comparable": bool(pairs) and not reasons, "reasons": sorted(set(reasons)),
            "tolerance": tolerance, "method": "paired median ratio; 10000 paired bootstrap resamples, seed 0; "
            "95% percentile interval expanded for worst-case histogram resolution; at least five valid pairs; "
            "descriptive per-metric intervals",
            "metrics": rows}


def measure(host, package, identity, output, case_mask):
    ManagementClient(host, timeout=30).deploy(package, name="bist")
    connection = connect(host, time.monotonic() + 5)
    try:
        if not connection.start_comparison(case_mask):
            raise RuntimeError("Controlled comparison start rejected")
        collect(connection, time.monotonic() + 20)
        report = download(connection, output)
        for key in ("server_sha256", "fpga_sha256"):
            if report["metadata"].get(key) != identity[key]:
                raise RuntimeError(f"Deployed {key} does not match intended package; raw report saved")
        return report
    finally:
        connection.close()


def run(args):
    started = time.monotonic()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=False)
    packages = {}
    for label in ("baseline", "candidate"):
        folder = args.output / label
        folder.mkdir()
        package = getattr(args, label)
        if package is None:
            package = build(getattr(args, label + "_ref"), folder, args.fpga_cache.resolve())
        else:
            target = folder / "bist.zip"
            shutil.copy2(package, target)
            package = target
        packages[label] = package
    identities = {label: package_identity(path) for label, path in packages.items()}
    save(args.output / "packages.json", identities)
    if identities["baseline"]["fpga_sha256"] != identities["candidate"]["fpga_sha256"]:
        raise ValueError("SDK comparisons require identical FPGA images")
    restore = packages["baseline"]
    if args.restore_package:
        folder = args.output / "restore"
        folder.mkdir()
        restore = folder / "bist.zip"
        shutil.copy2(args.restore_package, restore)
    # Validate the restoration archive before the first board mutation.
    package_identity(restore)
    print(f"Will restore {restore} after the comparison", flush=True)
    pairs = []
    try:
        for index in range(args.pairs):
            pair = {"pair": index + 1, "order": ["baseline", "candidate"] if index % 2 == 0 else ["candidate", "baseline"]}
            for label in pair["order"]:
                print(f"Pair {index + 1}/{args.pairs}: {label}", flush=True)
                pair[label] = measure(args.host, packages[label], identities[label],
                                      args.output / f"pair-{index + 1}" / label, args.case_mask)
            pairs.append(pair)
            result = summarize(pairs, args.tolerance)
            result["elapsed_seconds"] = time.monotonic() - started
            print(f"{('Provisional' if len(pairs) < 5 else 'Repeated')} results after {result['elapsed_seconds']:.1f} s", flush=True)
            save(args.output / "comparison.json", result)
            for row in result["metrics"]:
                if ".latency_us.p50" in row["name"] and "paired_ratio" in row:
                    print(f"  {row['name']}: {row['baseline_median_us']:.1f} → {row['candidate_median_us']:.1f} us "
                          f"({100 * (row['paired_ratio'] - 1):+.1f}%), {row['status']}", flush=True)
            if result["reasons"]:
                print("  Inconclusive: " + "; ".join(result["reasons"]), flush=True)
    finally:
        print("Restoring package", flush=True)
        try:
            ManagementClient(args.host, timeout=30).deploy(restore, name="bist")
        except Exception as error:
            save(args.output / "restore-error.json", {"error": str(error)})
            raise
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="192.168.1.84")
    for label in ("baseline", "candidate"):
        group = parser.add_mutually_exclusive_group(required=True)
        group.add_argument("--" + label, type=Path, help="Prebuilt BIST 1.3+ archive (identical probe source)")
        group.add_argument("--" + label + "-ref", help="SDK Git revision; build with this exact BIST source")
    parser.add_argument("--fpga-cache", type=Path, default=ROOT / ARTIFACTS,
                        help="Previously validated BIST FPGA outputs; required when building revisions")
    parser.add_argument("--restore-package", type=Path, help="Archive to restore; defaults to baseline package")
    parser.add_argument("--output", required=True, type=Path, help="New directory for packages and raw paired reports")
    parser.add_argument("--pairs", type=int, default=5)
    parser.add_argument("--cases", nargs="+", choices=CASES, default=["idle", "mixed"])
    parser.add_argument("--tolerance", type=float, default=.05)
    args = parser.parse_args()
    if args.pairs < 1 or not math.isfinite(args.tolerance) or not 0 < args.tolerance < 1:
        parser.error("pairs must be positive and tolerance must be finite between zero and one")
    args.case_mask = sum(1 << CASES.index(case) for case in set(args.cases))
    try:
        result = run(args)
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError, ManagementError) as error:
        parser.exit(1, f"BIST comparison: {error}\n")
    if not result["comparable"] or result["provisional"]:
        parser.exit(2, "Comparison remains inconclusive; inspect comparison.json\n")
    print(f"Saved {args.output / 'comparison.json'}")


if __name__ == "__main__":
    main()
