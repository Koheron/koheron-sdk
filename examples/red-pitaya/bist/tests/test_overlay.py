"""Validate the compiled overlay's actual bindings, rather than just its source."""
import subprocess
from pathlib import Path
import re


def test_compiled_overlay_bindings():
    root = Path(__file__).resolve().parents[4]
    overlay = root / "tmp/examples/red-pitaya/bist/stemlab-125-14/pl.dtbo"
    if not overlay.exists():
        import pytest
        pytest.skip("Build the instrument to verify its compiled device tree")
    text = subprocess.check_output(["dtc", "-I", "dtb", "-O", "dts", str(overlay)], stderr=subprocess.DEVNULL, text=True)
    for name in ("bist-uart-a", "bist-uart-b", "bist-dma", "bist-control", "bist-status", "bist-xadc"):
        assert f'linux,uio-name = "{name}"' in text
    for address in ("40600000", "40610000", "40400000", "40000000", "50000000"):
        node = re.search(rf'[^{{;]*@{address}\s*\{{(.*?)\n\s*\}};', text, re.S)
        assert node and 'compatible = "generic-uio"' in node[1]
    assert 'compatible = "koheron,mem-wc-1.0"' in text
    # Runtime-local labels must resolve inside this overlay, not be emitted as
    # external fixups against symbols in a previous instrument's base tree.
    fixups = text.split("__fixups__", 1)[1]
    assert "ctl_axi_ctl_register =" not in fixups
    assert "sts_axi_sts_register_0 =" not in fixups
    assert "uart_a =" not in fixups
    assert "dma =" not in fixups
