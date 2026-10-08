"""Verify SPL padding with binary data and both vendor makefile layouts."""
from pathlib import Path
import subprocess
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[2]


class UBootPatchTest(unittest.TestCase):
    def test_binary_padding_is_compatible_and_idempotent(self):
        for name in ('Makefile.spl', 'Makefile.xpl'):
            with self.subTest(makefile=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / 'scripts').mkdir()
                makefile = root / 'scripts' / name
                makefile.write_text('all:\n\tdd if=input.bin of=aligned.bin '
                                    'conv=block,sync bs=4 status=none\n')
                command = ['bash', str(SDK / 'os/scripts/patch_uboot.sh'), str(root)]
                subprocess.run(command, check=True)
                patched = makefile.read_text()
                self.assertNotIn('conv=block', patched)
                subprocess.run(command, check=True)
                self.assertEqual(makefile.read_text(), patched)
                for data in (b'', b'a', b'a\x00\n', b'a\x00\nb', b'a\x00\nbc'):
                    (root / 'input.bin').write_bytes(data)
                    result = subprocess.run(['make', '-f', str(makefile)], cwd=root,
                                            capture_output=True, text=True, timeout=10)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertEqual((root / 'aligned.bin').read_bytes(),
                                     data + bytes((-len(data)) % 4))


if __name__ == '__main__':
    unittest.main()
