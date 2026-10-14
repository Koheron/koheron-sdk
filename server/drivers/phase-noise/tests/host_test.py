"""Keep temporary C++ fixtures in the PNA runner's selected backend."""
import os


def fixture_command(sdk_path, directory, command):
    if os.environ.get('PNA_TEST_MODE', 'native') != 'docker':
        return command
    return ['docker', 'run', '--rm', '--pull=never', '--init',
            '-u', f'{os.getuid()}:{os.getgid()}',
            '-v', f'{sdk_path}:{sdk_path}', '-v', f'{directory}:{directory}',
            '-w', str(sdk_path), os.environ.get('PNA_CPP_IMAGE', 'cross-armhf:26.04'),
            *command]
