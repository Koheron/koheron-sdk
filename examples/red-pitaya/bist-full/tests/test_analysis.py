"""Independent known-answer checks for the code used to grade analog hardware."""

from pathlib import Path
import shutil
import subprocess

import pytest


def test_spectral_known_answers(tmp_path):
    compiler = shutil.which("g++")
    if compiler is None:
        pytest.skip("g++ is required for native measurement checks")
    source = Path(__file__).with_name("analysis.cpp")
    binary = tmp_path / "analysis"
    subprocess.run(
        [
            compiler,
            "-std=c++23",
            "-O2",
            "-Wall",
            "-Wextra",
            "-Werror",
            str(source),
            "-o",
            str(binary),
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    result = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
    assert "PASS spectral analysis:" in result.stdout
