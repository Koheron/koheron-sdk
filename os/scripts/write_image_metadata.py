"""Publish matching image manifests only when make requests a new build."""
from datetime import datetime, timezone
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def write_metadata(settings_path, manifest_path, release_path):
    settings = dict(line.split('=', 1) for line in settings_path.read_text().splitlines())
    timestamp = datetime.now(timezone.utc)
    if settings['BUILD_TIME_UTC']:
        timestamp = datetime.strptime(settings['BUILD_TIME_UTC'], '%Y%m%d.%H%M%S').replace(tzinfo=timezone.utc)
    build_id = settings['BUILD_ID'] or (
        timestamp.strftime('%Y%m%d.%H%M%S') + '.' + settings['GIT_COMMIT']
        + ('-dirty' if settings['GIT_DIRTY'] == 'yes' else ''))
    manifest = {
        'release': settings['RELEASE_NAME'], 'build_id': build_id,
        'project': settings['NAME'], 'board': settings['BOARD'],
        'zynq': settings['ZYNQ_TYPE'], 'kernel': settings['LINUX_TAG'],
        'u-boot': settings['UBOOT_TAG'], 'device-tree': settings['DTREE_TAG'],
        'compiler_image': settings['DOCKER_IMAGE'],
        'server_compiler': settings['GCC_ARCH'] + '-g++-' + settings['GCC_VERSION'],
        'kernel_compiler': settings['KERNEL_CC'], 'uboot_compiler': settings['UBOOT_CC'],
        'host_compiler': settings['OS_HOSTCC'],
    }
    tools = subprocess.run(['bash', str(Path(__file__).with_name('toolchain_versions.sh')),
                            settings['VIVADO_PATH'], settings['VITIS_PATH']],
                           check=True, text=True, stdout=subprocess.PIPE)
    manifest.update(line.split('=', 1) for line in tools.stdout.splitlines())
    manifest['koheron_version'] = settings['KOHERON_VERSION']
    manifest.update((key.lower(), settings[key]) for key in (
        'GIT_COMMIT', 'GIT_BRANCH', 'GIT_TAG', 'GIT_DESCRIBE', 'GIT_DIRTY'))
    manifest['generated_utc'] = timestamp.strftime('%Y-%m-%dT%H:%M:%SZ')
    release = {
        'NAME': 'Koheron', 'RELEASE': settings['RELEASE_NAME'], 'BUILD_ID': build_id,
        'UBUNTU': settings['UBUNTU_VERSION'], 'KOHERON_VERSION': settings['KOHERON_VERSION'],
    }
    release.update((key, settings[key]) for key in (
        'GIT_COMMIT', 'GIT_BRANCH', 'GIT_TAG', 'GIT_DESCRIBE', 'GIT_DIRTY'))

    staged = []
    try:
        for path, values in ((manifest_path, manifest), (release_path, release)):
            path.parent.mkdir(parents=True, exist_ok=True)
            with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8', dir=path.parent,
                                             prefix=path.name + '.tmp.', delete=False) as stream:
                temporary = Path(stream.name)
                staged.append((temporary, path))
                stream.write(''.join(f'{key}={value}\n' for key, value in values.items()))
            temporary.chmod(0o644)
        for temporary, path in staged:
            os.replace(temporary, path)
    finally:
        for temporary, _ in staged:
            temporary.unlink(missing_ok=True)


if __name__ == '__main__':
    write_metadata(*map(Path, sys.argv[1:]))
