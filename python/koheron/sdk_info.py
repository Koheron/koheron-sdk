"""Display the settings resolved by Make without building artifacts."""
import argparse
import io
import json
import os
from pathlib import Path
import subprocess


def read_settings(stream):
    data = json.load(stream)
    values = data.get('settings') if isinstance(data, dict) else None
    if (not isinstance(values, dict) or not all(isinstance(value, str) for value in values.values())
            or not all(values.get(key) for key in ('CONFIG_MK', 'MEMORY_YML'))):
        raise ValueError('Resolved settings must contain text values, including CONFIG_MK and MEMORY_YML paths')
    return values


def load_settings(sdk_path, cfg, overrides=()):
    result = subprocess.run(['make', '--no-print-directory', '--silent', 'info',
                             'FORMAT=json', f'CFG={cfg}', *overrides],
                            cwd=sdk_path, text=True, capture_output=True, timeout=15,
                            env={**os.environ, 'MAKEFLAGS': '', 'MFLAGS': ''})
    if result.returncode:
        raise ValueError(result.stderr.strip() or 'Make could not resolve CFG')
    return read_settings(io.StringIO(result.stdout))


def settings(values):
    variables = dict(value.split('=', 1) for value in values)
    path_keys = {'SDK_PATH', 'CONFIG_MK', 'BOARD_MK', 'MEMORY_YML', 'BD_TCL',
                 'VIVADO_PATH', 'VITIS_PATH', 'INSTRUMENT_ZIP', 'BOARD_INSTRUMENT_ZIP',
                 'BITSTREAM', 'SERVER', 'DRIVERS_JSON_OUT', 'RELEASE_ZIP',
                 'MANIFEST_TXT', 'INSTRUMENT_TEST'}
    for key in path_keys:
        if variables.get(key):
            variables[key] = str(Path(variables[key]).absolute())
    return {'schema_version': 1, 'settings': variables}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--format', choices=('text', 'json'), default='text')
    parser.add_argument('variables', nargs='*', metavar='KEY=VALUE')
    args = parser.parse_args()
    result = settings(args.variables)
    if args.format == 'json':
        print(json.dumps(result, indent=2))
    else:
        values = result['settings']
        for label, keys in (
            ('Instrument', ('NAME', 'VERSION', 'CONFIG_MK')),
            ('Board', ('BOARD', 'PART', 'UBUNTU_ARCH')),
            ('Inputs', ('MEMORY_YML', 'BD_TCL', 'DRIVERS', 'WEB_FILES', 'INSTRUMENT_TEST')),
            ('Tools', ('VIVADO_VERSION', 'VIVADO_PATH', 'VITIS_PATH', 'GCC_ARCH',
                       'GCC_VERSION', 'KERNEL_CC', 'UBOOT_CC', 'DOCKER_IMAGE', 'WEB_DOCKER_IMAGE')),
            ('Build', ('MODE', 'FPGA_SYNTH_MODE', 'ENFORCE_TIMING', 'N_CPUS',
                       'LINUX_VERSION', 'UBOOT_VERSION', 'UBUNTU_VERSION')),
            ('Artifacts', ('INSTRUMENT_ZIP', 'BOARD_INSTRUMENT_ZIP', 'RELEASE_ZIP',
                           'BITSTREAM', 'SERVER', 'DRIVERS_JSON_OUT', 'MANIFEST_TXT')),
        ):
            print(label + ':')
            for key in keys:
                value = values.get(key)
                if key in ('DRIVERS', 'WEB_FILES') and value:
                    print(f'  {key}:')
                    for path in value.split():
                        print(f'    {path}')
                else:
                    print(f'  {key:<22} {value or "(unset)"}')


if __name__ == '__main__':
    main()
