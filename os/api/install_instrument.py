"""Stage an instrument and restore the previous installation on load failure."""
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import sys
import tempfile
import zipfile

SERVICE = 'koheron-server.service'
IDENTITY = '.instrument-name'


def systemctl(action, unit=SERVICE, *, check=True):
    return subprocess.run(['/bin/systemctl', action, unit], check=check)


def stage_archive(filename, destination):
    """Check and extract before touching the running installation."""
    with zipfile.ZipFile(filename) as archive:
        names = set()
        for member in archive.infolist():
            path = PurePosixPath(member.filename)
            mode = member.external_attr >> 16
            if (path.is_absolute() or '..' in path.parts or '\\' in member.filename
                    or str(path) in names or path.name == IDENTITY
                    or stat.S_IFMT(mode) not in (0, stat.S_IFREG, stat.S_IFDIR)):
                raise ValueError('Unsafe or duplicate archive member: ' + member.filename)
            names.add(str(path))
        required = {'version', 'serverd'}
        if not required <= names:
            raise ValueError('Instrument archive requires version and serverd')
        if archive.testzip() is not None:
            raise ValueError('Corrupt instrument archive')
        archive.extractall(destination)
        for member in archive.infolist():
            if not member.is_dir():
                # ZipFile does not restore Unix permissions; preserve executable bits.
                mode = member.external_attr >> 16
                (destination / member.filename).chmod(0o644 | (mode & 0o111))
    # Current overlay packages and legacy xdevcfg packages are supported.
    def payload(pattern):
        return any(p.is_file() and p.stat().st_size > 0 for p in destination.glob(pattern))
    if not (payload('*.bit') or (payload('pl.dtbo') and payload('*.bit.bin'))):
        raise ValueError('Instrument archive has no FPGA payload')
    if not (destination / 'serverd').is_file() or not os.access(destination / 'serverd', os.X_OK):
        raise ValueError('Instrument serverd is not executable')
    if not (destination / 'version').read_text().strip():
        raise ValueError('Instrument version is empty')
    (destination / IDENTITY).write_text(Path(filename).stem + '\n')


def install(filename, live):
    live = Path(live)
    # A sibling staging directory keeps directory renames on one filesystem.
    transaction = Path(tempfile.mkdtemp(prefix='.instrument-', dir=live.parent))
    staged = transaction / 'next'
    backup = transaction / 'previous'
    staged.mkdir()
    try:
        stage_archive(filename, staged)
        was_active = systemctl('is-active', check=False).returncode == 0
        systemctl('stop')
        activated = False
        try:
            if live.exists():
                live.rename(backup)
            staged.rename(live)
            activated = True
            systemctl('start')  # Type=notify waits for the server's READY=1.
        except Exception:
            # Stop any partially started replacement before restoring its files.
            if activated:
                systemctl('stop')
                shutil.rmtree(live)
            if backup.exists():
                backup.rename(live)
            if was_active:
                systemctl('start')
            raise
        # The new server is ready. Failure of the cosmetic LED helper must not
        # turn a successful instrument load into a reported installation failure.
        if backup.exists():
            shutil.rmtree(backup)
        try:
            systemctl('start', 'koheron-server-init.service')
        except (OSError, subprocess.CalledProcessError) as exc:
            print('Instrument loaded; LED initialization failed: ' + str(exc), file=sys.stderr)
    finally:
        if backup.exists():
            print('Recovery failed; previous files retained at ' + str(backup), file=sys.stderr)
        else:
            shutil.rmtree(transaction)


if __name__ == '__main__':
    try:
        install(Path(sys.argv[1]), Path(sys.argv[2]))
    except Exception as exc:
        print('Instrument installation failed: ' + str(exc), file=sys.stderr)
        sys.exit(1)
