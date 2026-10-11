"""Cache paths and clean targets remain scoped to the selected build cache."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


SDK = Path(__file__).resolve().parents[2]


class CachePathTest(unittest.TestCase):
    def make(self, *args, success=True):
        result = subprocess.run(['make', '--no-print-directory', *args], cwd=SDK,
                                capture_output=True, text=True, timeout=15,
                                env={**os.environ, 'MAKEFLAGS': '', 'MFLAGS': ''})
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result

    def test_equivalent_relative_config_paths_share_a_safe_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / 'cache'
            cfg = 'examples/red-pitaya/led-blinker/config.mk'
            alternate = f'../{SDK.name}/{cfg}'
            paths = []
            for selection in (cfg, alternate, str(SDK / cfg)):
                values = json.loads(self.make('info', f'CFG={selection}',
                                              f'TMP={cache}', 'FORMAT=json').stdout)['settings']
                paths.append(values['INSTRUMENT_ZIP'])
            self.assertEqual(paths, [str(cache / 'examples/red-pitaya/led-blinker/led-blinker.zip')] * 3)
            self.assertFalse(cache.exists())

    def test_external_config_clean_stays_within_cache_and_rejects_override_escape(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config_dir = root / 'instrument'
            config_dir.mkdir()
            config = config_dir / 'config.mk'
            config.write_text('NAME := external\nVERSION := 0.1\n'
                              f'BOARD_PATH := {SDK}/boards/red-pitaya\n')
            relative_cfg = os.path.relpath(config, SDK)
            cache = root / 'cache'
            values = json.loads(self.make('info', f'CFG={relative_cfg}',
                                          f'TMP={cache}', 'FORMAT=json').stdout)['settings']
            archive = Path(values['INSTRUMENT_ZIP'])
            self.assertEqual(archive.parent.parent, cache / '.external')
            self.assertEqual(len(archive.parent.name), 64)
            archive.parent.mkdir(parents=True)
            archive.write_text('cached')
            outside = root / 'outside'
            outside.mkdir()
            (outside / 'sentinel').write_text('keep')
            self.make('clean', f'CFG={relative_cfg}', f'TMP={cache}',
                      f'TMP_PROJECT_PATH={outside}', success=False)
            self.assertTrue((outside / 'sentinel').exists())
            cache.mkdir(exist_ok=True)
            (cache / 'linked').symlink_to(outside, target_is_directory=True)
            self.make('clean', f'CFG={relative_cfg}', f'TMP={cache}',
                      f'TMP_PROJECT_PATH={cache / "linked"}', success=False)
            self.assertTrue((outside / 'sentinel').exists())
            self.make('clean', f'CFG={relative_cfg}', f'TMP={cache}')
            self.assertFalse(archive.parent.exists())
            self.assertTrue(config.exists())
            self.assertTrue((outside / 'sentinel').exists())
            cache.mkdir(exist_ok=True)
            (cache / 'marker').touch()
            self.make('clean_all', f'TMP={cache}')
            self.assertFalse(cache.exists())
            self.assertTrue(config.exists())

    def test_clean_all_rejects_sdk_root(self):
        result = self.make('clean_all', 'TMP=.', success=False)
        self.assertIn('Refusing to clean cache root containing the SDK', result.stderr)
        self.assertTrue((SDK / 'Makefile').exists())


if __name__ == '__main__':
    unittest.main()
