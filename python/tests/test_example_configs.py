"""Named board selectors must be visible to users, host checks and CI."""
import contextlib
import io
from pathlib import Path
import tempfile
import unittest

from koheron.list_examples import find_configs, list_examples


class ExampleConfigTest(unittest.TestCase):
    def test_discovers_named_selectors_and_legacy_configs_without_fragments(self):
        with tempfile.TemporaryDirectory() as directory:
            sdk = Path(directory)
            instrument = sdk / 'examples/red-pitaya/fft'
            instrument.mkdir(parents=True)
            selectors = [instrument / f'{board}.mk' for board in
                         ('stemlab-125-14', 'stemlab-125-14-pro-gen2')]
            for selected in selectors:
                selected.write_text('NAME := fft\nBOARD_PATH := $(SDK_PATH)/boards/board\n'
                                    'include $(PROJECT_PATH)/common.mk\n')
            (instrument / 'common.mk').write_text('VERSION := 1\n')
            (instrument / 'config_1g.mk').write_text('BOARD_PATH := $(SDK_PATH)/boards/other\n')
            legacy = sdk / 'examples/alpha250/fft/config.mk'
            legacy.parent.mkdir(parents=True)
            legacy.write_text('NAME := fft\n')
            self.assertEqual(find_configs(sdk), sorted([legacy, *selectors]))
            stream = io.StringIO()
            with contextlib.redirect_stdout(stream):
                list_examples(sdk)
            for selected in selectors:
                self.assertIn(selected.relative_to(sdk).as_posix(), stream.getvalue())
                self.assertIn(f'[{selected.stem}]', stream.getvalue())
            self.assertNotIn('common.mk', stream.getvalue())
