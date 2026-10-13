"""Validate the memory map selected by CFG, including maps shared by boards."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('validate_config', SDK / 'python/koheron/validate_config.py')
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)


class MemoryPathTest(unittest.TestCase):
    def test_selected_map_overrides_local_map(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'board').mkdir()
            (root / 'board/board.mk').write_text('')
            (root / 'shared.yml').write_text('memory: []\n')
            (root / 'memory.yml').write_text('this is not a memory map\n')
            config = root / 'config.mk'
            config.write_text('NAME := test\nVERSION := 1\nBOARD_PATH := $(SDK_PATH)/board\nMEMORY_YML := $(SDK_PATH)/shared.yml\n')
            self.assertEqual(validator.main(['--sdk-path', str(root), '--cfg', str(config)]), 0)
            (root / 'shared.yml').write_text('invalid selected map\n')
            self.assertEqual(validator.main(['--sdk-path', str(root), '--cfg', str(config)]), 1)

    def test_default_map_is_local(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'board').mkdir()
            (root / 'board/board.mk').write_text('')
            (root / 'memory.yml').write_text('memory: []\n')
            config = root / 'config.mk'
            config.write_text('NAME := test\nVERSION := 1\nBOARD_PATH := $(SDK_PATH)/board\n')
            self.assertEqual(validator.main(['--sdk-path', str(root), '--cfg', str(config)]), 0)


if __name__ == '__main__':
    unittest.main()
