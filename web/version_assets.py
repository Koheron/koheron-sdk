"""Give management CSS/JS URLs content versions to bypass older cached assets."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import tempfile


ASSETS = ('instruments.js', 'main.css', 'instrument.css', 'system.css')
PATTERN = re.compile(r'/koheron/(' + '|'.join(re.escape(name) for name in ASSETS) + r""")(?=["'])""")


def version_assets(source, assets, destination):
    versions = {name: hashlib.sha256((assets / name).read_bytes()).hexdigest()[:16] for name in ASSETS}
    html = PATTERN.sub(lambda match: match.group(0) + '?v=' + versions[match.group(1)], source.read_text(encoding='utf-8'))
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8', dir=destination.parent,
                                         prefix='.' + destination.name + '.', delete=False) as output:
            temporary = Path(output.name)
            output.write(html)
        temporary.chmod(0o644)
        os.replace(temporary, destination)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('assets', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    version_assets(args.source, args.assets, args.destination)
