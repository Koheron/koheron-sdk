"""Compile/apply the actual ALPHA250 boot overlay and check DMA ownership."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def run(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout.strip()


with tempfile.TemporaryDirectory(prefix="alpha250-dma-reservation-") as directory:
    output = Path(directory)
    base = output / "base.dts"
    base.write_text("""/dts-v1/;
/ {
    #address-cells = <1>;
    #size-cells = <1>;
    spi0: spi@e0006000 { reg = <0xe0006000 0x1000>; };
    spi1: spi@e0007000 { reg = <0xe0007000 0x1000>; };
    usb0: usb@e0002000 { reg = <0xe0002000 0x1000>; };
};
""")
    run("dtc", "-@", "-I", "dts", "-O", "dtb", "-o", str(output / "base.dtb"), str(base))
    run("dtc", "-@", "-I", "dts", "-O", "dtb", "-o", str(output / "board.dtbo"),
        str(ROOT / "boards/alpha250/config/board.dtso"))
    run("fdtoverlay", "-i", str(output / "base.dtb"), "-o", str(output / "merged.dtb"),
        str(output / "board.dtbo"))
    tree = str(output / "merged.dtb")
    node = "/reserved-memory/dma-buffer@18000000"
    assert run("fdtget", "-t", "x", tree, "/memory@0", "reg").split() == ["0", "20000000"]
    assert run("fdtget", "-t", "x", tree, node, "reg").split() == ["18000000", "8000000"]
    assert run("fdtget", tree, node, "no-map") == ""
    for forbidden in ("reusable", "linux,cma", "compatible"):
        assert subprocess.run(["fdtget", tree, node, forbidden], capture_output=True).returncode != 0
    print("ALPHA250 boot overlay excludes the entire fixed DMA ring from Linux RAM")
