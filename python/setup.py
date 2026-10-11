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
    python_requires='>=3.9',
    install_requires=['numpy>=1.26', 'requests', 'Click', 'websocket-client>=1.8,<2'],
    classifiers=[
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3 :: Only'
    ],
    entry_points='''
        [console_scripts]
        koheron=koheron.cli:cli
    ''',
)
