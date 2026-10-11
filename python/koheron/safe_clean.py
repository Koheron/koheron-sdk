"""Remove a build cache only when its resolved path is safely scoped."""
import argparse
from pathlib import Path
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk-path', type=Path, required=True)
    parser.add_argument('--root', type=Path, required=True)
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument('--target', type=Path)
    selection.add_argument('--all', action='store_true')
    args = parser.parse_args()

    sdk = args.sdk_path.resolve()
    root = args.root.resolve()
    if root == sdk or root in sdk.parents:
        parser.error(f'Refusing to clean cache root containing the SDK: {root}')
    target = args.target if args.target is not None else args.root
    resolved = target.resolve()
    if args.target is not None and root not in resolved.parents:
        parser.error(f'Refusing to clean outside cache root {root}: {resolved}')
    if target.is_symlink() or target.is_file():
        target.unlink()
    elif target.exists():
        shutil.rmtree(target)


if __name__ == '__main__':
    main()
