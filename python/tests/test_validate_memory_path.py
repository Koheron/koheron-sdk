"""Validation must check the same configuration and overrides as Make."""
import contextlib
import importlib.util
import io
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

SDK = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('validate_config', SDK / 'python/koheron/validate_config.py')
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)


class ResolvedConfigTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.config = self.root / 'config.mk'
        self.header = ('NAME := test\nVERSION := 1\n'
                       'BOARD_PATH := $(SDK_PATH)/boards/zedboard\n')
        self.config.write_text(self.header)
        (self.root / 'memory.yml').write_text('memory: []\n')

    def validate(self):
        stream = io.StringIO()
        with contextlib.redirect_stdout(stream):
            code = validator.main(['--sdk-path', str(SDK), '--cfg', str(self.config)])
        return code, stream.getvalue()

    def test_default_map_is_local(self):
        code, output = self.validate()
        self.assertEqual(code, 0, output)
        self.assertIn(str(self.root / 'memory.yml'), output)

    def test_selected_map_overrides_local_map(self):
        (self.root / 'shared.yml').write_text('memory: []\n')
        (self.root / 'memory.yml').write_text('invalid local map\n')
        self.config.write_text(self.header + 'MEMORY_YML := $(PROJECT_PATH)/shared.yml\n')
        code, output = self.validate()
        self.assertEqual(code, 0, output)
        (self.root / 'shared.yml').write_text('invalid selected map\n')
        self.assertEqual(self.validate()[0], 1)

    def test_deferred_variables_functions_and_inactive_branches(self):
        (self.root / 'shared.yml').write_text('memory: []\n')
        (self.root / 'memory.yml').write_text('invalid local map\n')
        (self.root / 'selection.mk').write_text('MAP := shared\n')
        self.config.write_text(self.header + '''MEMORY_YML = $(abspath $(PROJECT_PATH)/$(MAP).yml)
include $(PROJECT_PATH)/selection.mk
-include $(PROJECT_PATH)/optional.mk
ifeq ($(MAP),unused)
include $(PROJECT_PATH)/missing.mk
MEMORY_YML := missing.yml
DRIVERS += missing.hpp
endif
''')
        code, output = self.validate()
        self.assertEqual(code, 0, output)
        self.assertIn(str(self.root / 'shared.yml'), output)

    def test_only_final_driver_selection_is_validated(self):
        (self.root / 'driver.hpp').touch()
        self.config.write_text(self.header + '''DRIVERS += $(PROJECT_PATH)/retired.hpp
DRIVERS := $(filter-out %/retired.hpp,$(DRIVERS))
DRIVERS += $(addprefix $(PROJECT_PATH)/,driver.hpp)
''')
        code, output = self.validate()
        self.assertEqual(code, 0, output)
        (self.root / 'driver.hpp').unlink()
        code, output = self.validate()
        self.assertEqual(code, 1)
        self.assertIn(f'DRIVERS file missing: {self.root}/driver.hpp', output)

    def test_make_validate_preserves_command_line_overrides(self):
        stamp = self.root / 'requirements.stamp'
        stamp.touch()
        selected = self.root / 'selected.yml'
        selected.write_text('memory: []\n')
        (self.root / 'memory.yml').write_text('invalid local map\n')
        command = ['make', '--no-print-directory', 'validate', f'CFG={self.config}',
                   f'PYTHON_REQUIREMENTS_STAMP={stamp}', f'HOST_PYTHON={sys.executable}',
                   f'VENV={Path(sys.executable).parent.parent}', f'MEMORY_YML={selected}']
        env = {**os.environ, 'MAKEFLAGS': '', 'MFLAGS': '', 'MAKEOVERRIDES': ''}
        result = subprocess.run(command, cwd=SDK, env=env, text=True, capture_output=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for setting, message in ((f'DRIVERS={self.root}/missing.hpp', 'DRIVERS file missing'),
                                 (f'XDC={self.root}/missing.xdc', 'XDC file missing'),
                                 ('VERSION=', 'VERSION is missing or empty')):
            with self.subTest(setting=setting):
                result = subprocess.run([*command, setting], cwd=SDK, env=env,
                                        text=True, capture_output=True, timeout=15)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn(message, result.stdout)

    def test_invalid_settings_and_missing_includes_are_reported_without_tracebacks(self):
        with contextlib.redirect_stdout(io.StringIO()) as output, patch.object(sys, 'stdin', io.StringIO('{}')):
            self.assertEqual(validator.main(['--settings-stdin']), 1)
        self.assertIn('Cannot resolve configuration', output.getvalue())
        self.config.write_text(self.header + 'include $(PROJECT_PATH)/missing.mk\n')
        code, output = self.validate()
        self.assertEqual(code, 1)
        self.assertIn('missing.mk', output)
        self.assertNotIn('Traceback', output)


if __name__ == '__main__':
    unittest.main()
