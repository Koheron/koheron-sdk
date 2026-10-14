"""The UI must show report identities, including older persisted reports."""
import json
from pathlib import Path
import shutil
import subprocess

import pytest


def rows(metadata):
    if not shutil.which("node"):
        pytest.skip("Node is required for the metadata presentation check")
    script = Path(__file__).parents[1] / "web/bist-metadata.js"
    runner = """
const fs = require('fs'), vm = require('vm');
const context = {window: {}};
vm.runInNewContext(fs.readFileSync(process.argv[1], 'utf8'), context);
console.log(JSON.stringify(context.window.bistMetadata.rows(JSON.parse(process.argv[2]))));
"""
    return json.loads(subprocess.check_output(
        ["node", "-e", runner, str(script), json.dumps(metadata)], text=True))


def test_versions_and_image_build_are_visible():
    result = rows({"instrument_name": "bist", "instrument_version": "1.1.0",
                   "sdk_version": "1.0", "kernel": "6.18.0-xilinx",
                   "os": {"PRETTY_NAME": "Ubuntu 26.04.1 LTS"},
                   "os_image": {"RELEASE": "red-pitaya-fft", "BUILD_ID": "image-build",
                                "GIT_COMMIT": "os-commit"},
                   "instrument_manifest": '{"sdk_commit":"sdk-commit"}'})
    summary, details = dict(result["summary"]), dict(result["details"])
    assert summary["Instrument"] == "bist 1.1.0"
    assert summary["Operating system"] == "Ubuntu 26.04.1 LTS"
    assert summary["SDK version"] == "1.0"
    assert summary["Kernel"] == "6.18.0-xilinx"
    assert summary["OS image"] == "red-pitaya-fft"
    assert details["OS build"] == "image-build"
    assert details["OS source"] == "os-commit"
    assert details["SDK source"] == "sdk-commit"


def test_legacy_report_and_missing_metadata():
    result = rows({"bist_version": "1.0.0", "os_release": 'PRETTY_NAME="Ubuntu 26.04.1 LTS"\n',
                   "instrument_manifest": "interrupted"})
    assert dict(result["summary"])["Instrument"] == "bist 1.0.0"
    assert dict(result["summary"])["Operating system"] == "Ubuntu 26.04.1 LTS"
    assert dict(result["details"])["SDK source"] == "Unavailable"
    assert dict(rows({})["summary"])["Kernel"] == "Unavailable"
