#!/usr/bin/env python3
"""Generate common legal IQ vectors, or assess the two latency-aligned outputs."""
import json
import math
import sys
from pathlib import Path

CIRCLE_SAMPLES = 131072

if len(sys.argv) != 4 or sys.argv[1] not in {"vectors", "report"}:
    raise SystemExit("Usage: pna_compare.py vectors|report input.txt output.txt|json")
source, destination = map(Path, sys.argv[2:])
if sys.argv[1] == "vectors":
    # Vendor Cartesian inputs have two integer bits: radius <= 2^22 is 1.0.
    rows = []
    for line in source.read_text().splitlines():
        x, y = map(int, line.split()[:2])
        if x*x + y*y <= 2**44:
            rows.append(line)
    for k in range(CIRCLE_SAMPLES):
        theta = 2*math.pi*k/CIRCLE_SAMPLES
        x, y = round(768000*math.cos(theta)), round(768000*math.sin(theta))
        phase = math.atan2(y, x)*8192/math.pi
        rows.append(f"{x} {y} {math.floor(phase+0.5)} 1 1 {phase:.12f}")
    destination.write_text("\n".join(rows)+"\n")
    print(f"Generated {len(rows)} matched PNA comparison vectors")
else:
    outputs = {"C": [], "V": []}
    for line in source.read_text().splitlines():
        kind, x, y, actual, reference = line.split()
        x, y, actual = int(x), int(y), int(actual)
        exact = float(reference)*math.pi/8192
        error = (actual*math.pi/2**21-exact+math.pi) % (2*math.pi)-math.pi
        outputs[kind].append((x, y, exact, error))
    assert len(outputs["C"]) == len(outputs["V"]) > CIRCLE_SAMPLES
    for custom, vendor in zip(outputs["C"], outputs["V"]):
        assert custom[:3] == vendor[:3], "Input/reference alignment mismatch"
    assert max(abs(row[3]) for row in outputs["C"]) < 1.5e-6

    def stats(rows):
        return {"samples": len(rows), "peak_urad": max(abs(r[3]) for r in rows)*1e6,
                "rms_urad": math.sqrt(sum(r[3]**2 for r in rows)/len(rows))*1e6}

    report = {"matched_samples": len(outputs["C"]),
              "custom_latency_clocks": 14, "pna_latency_clocks": 28}
    for kind, rows in outputs.items():
        report[f"nonzero_{kind}"] = stats([r for r in rows if r[0] or r[1]])
        report[f"radius_at_least_524288_{kind}"] = stats(
            [r for r in rows if r[0]**2+r[1]**2 >= 524288**2])
        report[f"circle_{kind}"] = stats(rows[-CIRCLE_SAMPLES:])
    destination.write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report, indent=2))
