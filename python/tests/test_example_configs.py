"""Named board selectors must be visible to users, host checks and CI."""
import contextlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from koheron.list_examples import find_configs, list_examples

SDK = Path(__file__).resolve().parents[2]


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
            # Discovery remains available when settings are inherited or broken.
            selectors[0].write_text('include missing.mk\n')
            self.assertEqual(find_configs(sdk), sorted([legacy, *selectors]))

    def test_listing_resolves_inherited_values_and_board_identity(self):
        selected = SDK / 'examples/red-pitaya/fft/stemlab-125-14-pro-gen2.mk'
        stream = io.StringIO()
        with contextlib.redirect_stdout(stream), patch('koheron.list_examples.find_configs', return_value=[selected]):
            self.assertEqual(list_examples(SDK), 0)
        output = stream.getvalue()
        self.assertIn(selected.relative_to(SDK).as_posix(), output)
        self.assertIn('[stemlab-125-14-pro-gen2]', output)
        self.assertIn('VERSION   0.3.0', output)
        self.assertIn('BOARD     red-pitaya-pro-gen2', output)
        self.assertNotIn('$(SDK_PATH)', output)

    def test_listing_reports_a_broken_config_and_continues(self):
        broken = SDK / 'examples/missing/config.mk'
        good = SDK / 'examples/zedboard/led-blinker/config.mk'
        stream, errors = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stream), contextlib.redirect_stderr(errors), \
             patch('koheron.list_examples.find_configs', return_value=[broken, good]):
            self.assertEqual(list_examples(SDK), 1)
        self.assertIn('Cannot resolve examples/missing/config.mk', errors.getvalue())
        self.assertIn('NAME      led-blinker', stream.getvalue())
