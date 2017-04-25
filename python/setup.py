from setuptools import setup, find_packages

__version__ = None
with open('koheron/version.py') as f:
    exec(f.read())

setup(
    name='koheron',
    version=str(__version__),
    author='Koheron',
    author_email='hello@koheron.com',
    url='https://github.com/Koheron/koheron-sdk',
    packages=find_packages(),
    license='MIT',
    description='Koheron Python Library',
    long_description='Please see our GitHub README',
    keywords='FPGA Linux Instrumentation',
    install_requires=['requests', 'Click'],
    classifiers=[
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3.5'
    ],
    entry_points='''
        [console_scripts]
        koheron=koheron.cli:cli
    ''',
)
