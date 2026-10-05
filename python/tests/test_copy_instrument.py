"""Host-only tests: python3 -m unittest discover -s python/tests -p test_copy_instrument.py."""

from __future__ import annotations

import importlib.util
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

SDK_PATH = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "copy_instrument", SDK_PATH / "python/koheron/copy_instrument.py"
)
assert spec is not None and spec.loader is not None
copy_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(copy_module)


class CopyInstrumentTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "examples/board/original"
        self.source.mkdir(parents=True)
        self.cfg = self.source / "config.mk"
        self.config = (
            "NAME := original # package name\nVERSION := 0.2.1\n"
            "BOARD_PATH := $(SDK_PATH)/boards/board\n"
            "DRIVERS += $(PROJECT_PATH)/original.hpp\n"
            "include missing-build-only-dependency.mk\n"
        )
        self.cfg.write_text(self.config)
        (self.source / "original.hpp").write_text("class Original {};\n")
        (self.source / "web").mkdir()
        (self.source / "web/index.html").write_text("<title>Original</title>\n")
        (self.source / ".source-settings").write_text("keep\n")
        self.destination = self.root / "examples/board/my-instrument"

    def copy(self) -> Path:
        return copy_module.copy_instrument(self.root, self.cfg, self.destination)

    def test_copies_sources_and_changes_only_package_name(self) -> None:
        copied_cfg = self.copy()
        self.assertEqual(copied_cfg, self.destination / "config.mk")
        self.assertEqual(copied_cfg.read_text(), self.config.replace("NAME := original", "NAME := my-instrument"))
        for relative in ("original.hpp", "web/index.html", ".source-settings"):
            self.assertEqual((self.destination / relative).read_bytes(), (self.source / relative).read_bytes())
        self.assertEqual(self.cfg.read_text(), self.config)

    def test_preserves_assignment_operator_comments_and_line_endings(self) -> None:
        for index, operator in enumerate((":=", "=", "?=")):
            with self.subTest(operator=operator):
                original = f"\tNAME {operator} original\t# name\r\nVERSION := 1\r\n".encode()
                self.cfg.write_bytes(original)
                self.destination = self.root / f"copy-{index}"
                copied_cfg = self.copy()
                self.assertEqual(copied_cfg.read_bytes(), original.replace(b"original", self.destination.name.encode()))
                self.assertEqual(self.cfg.read_bytes(), original)

    def test_resolves_relative_paths_against_sdk_path(self) -> None:
        copied_cfg = copy_module.copy_instrument(
            self.root, self.cfg.relative_to(self.root), self.destination.relative_to(self.root)
        )
        self.assertEqual(copied_cfg, self.destination / "config.mk")
        self.assertIn("NAME := my-instrument", copied_cfg.read_text())

    def test_skips_generated_dependencies_and_caches(self) -> None:
        for directory in (".git", ".venv", "__pycache__", "node_modules", ".Xil", ".cache", ".pytest_cache"):
            path = self.source / directory
            path.mkdir()
            (path / "generated").write_text("skip")
        (self.source / "module.pyc").write_bytes(b"cache")
        self.copy()
        self.assertEqual(sorted(path.name for path in self.destination.iterdir()),
                         [".source-settings", "config.mk", "original.hpp", "web"])

    def test_refuses_existing_directory_file_and_broken_symlink(self) -> None:
        destinations = (self.root / "existing-dir", self.root / "existing-file", self.root / "broken-link")
        destinations[0].mkdir()
        (destinations[0] / "sentinel").write_text("keep")
        destinations[1].write_text("keep")
        destinations[2].symlink_to(self.root / "absent")
        for destination in destinations:
            with self.subTest(destination=destination):
                self.destination = destination
                with self.assertRaisesRegex(ValueError, "already exists"):
                    self.copy()
        self.assertEqual((destinations[0] / "sentinel").read_text(), "keep")
        self.assertEqual(destinations[1].read_text(), "keep")
        self.assertTrue(destinations[2].is_symlink())

    def test_refuses_recursive_destination_including_through_symlink(self) -> None:
        alias = self.root / "source-alias"
        alias.symlink_to(self.source, target_is_directory=True)
        for destination in (self.source / "new", alias / "new"):
            with self.subTest(destination=destination):
                self.destination = destination
                with self.assertRaisesRegex(ValueError, "outside"):
                    self.copy()
                self.assertFalse(destination.exists())

    def test_refuses_invalid_or_unchanged_instrument_name(self) -> None:
        for name in ("bad name", "bad.name", "-bad", "original"):
            with self.subTest(name=name):
                self.destination = self.root / "new-parent" / name
                with self.assertRaises(ValueError):
                    self.copy()
                self.assertFalse(self.destination.parent.exists())

    def test_refuses_missing_or_nonliteral_or_multiple_name_assignments(self) -> None:
        for config in ("VERSION := 1\n", "NAME := $(OTHER)\n", "NAME := one\nNAME := two\n", "NAME += one\n"):
            with self.subTest(config=config):
                self.cfg.write_text(config)
                with self.assertRaisesRegex(ValueError, "literal NAME"):
                    self.copy()
                self.assertFalse(self.destination.exists())
                self.assertEqual(self.cfg.read_text(), config)

    def test_refuses_missing_or_wrong_configuration_file(self) -> None:
        for config in (self.source / "missing/config.mk", self.source / "original.hpp"):
            with self.subTest(config=config):
                with self.assertRaisesRegex(ValueError, "existing config.mk"):
                    copy_module.copy_instrument(self.root, config, self.destination)
                self.assertFalse(self.destination.exists())

    def test_symlinked_config_does_not_modify_original(self) -> None:
        external_cfg = self.root / "shared.mk"
        self.cfg.rename(external_cfg)
        self.cfg.symlink_to(external_cfg)
        self.copy()
        self.assertEqual(external_cfg.read_text(), self.config)
        self.assertTrue(self.cfg.is_symlink())
        self.assertFalse((self.destination / "config.mk").is_symlink())
        self.assertIn("NAME := my-instrument", (self.destination / "config.mk").read_text())

    def test_preserves_other_symlinks(self) -> None:
        (self.source / "shared.hpp").symlink_to("original.hpp")
        self.copy()
        self.assertTrue((self.destination / "shared.hpp").is_symlink())
        self.assertEqual((self.destination / "shared.hpp").read_text(), "class Original {};\n")

    def test_removes_partial_copy_on_failure(self) -> None:
        def fail_copy(*args: object, **kwargs: object) -> None:
            (self.destination / "partial").write_text("incomplete")
            raise OSError("copy failed")

        with patch.object(copy_module.shutil, "copytree", side_effect=fail_copy):
            with self.assertRaisesRegex(OSError, "copy failed"):
                self.copy()
        self.assertFalse(self.destination.exists())
        self.assertEqual(self.cfg.read_text(), self.config)

    def test_make_copy_without_loading_build_configuration(self) -> None:
        result = subprocess.run(
            ["make", "--no-print-directory", "copy", f"CFG={self.cfg}", f"DEST={self.destination}"],
            cwd=SDK_PATH, capture_output=True, text=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("NAME := my-instrument", result.stdout)
        self.assertEqual(self.cfg.read_text(), self.config)
        repeated = subprocess.run(
            ["make", "--no-print-directory", "copy", f"CFG={self.cfg}", f"DEST={self.destination}"],
            cwd=SDK_PATH, capture_output=True, text=True, check=False,
        )
        self.assertNotEqual(repeated.returncode, 0)
        self.assertIn("already exists", repeated.stderr)
        self.assertNotIn("Traceback", repeated.stderr)

    def test_make_copy_requires_cfg_and_destination(self) -> None:
        for arguments in ([], [f"CFG={self.cfg}"]):
            with self.subTest(arguments=arguments):
                result = subprocess.run(
                    ["make", "--no-print-directory", "copy", *arguments],
                    cwd=SDK_PATH, capture_output=True, text=True, check=False,
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(self.destination.exists())


if __name__ == "__main__":
    unittest.main()
