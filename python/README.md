# koheron-python

[![CircleCI](https://circleci.com/gh/Koheron/koheron-python.svg?style=shield)](https://circleci.com/gh/Koheron/koheron-python)
[![PyPI version](https://badge.fury.io/py/koheron.svg)](https://badge.fury.io/py/koheron)

Koheron Python Library

### Install from a specific tag

```
pip install https://github.com/Koheron/koheron-python/zipball/v0.11.0
```
### Develop in a virtualenv

```bash
virtualenv venv
source venv/bin/activate
pip install -e . # Install koheron package
```


### Running tests

Testing server emission/reception:
```sh
make start_koheron_server
make test
```
This test runs locally and starts a server in background. 
The tests are run in virtualenvs (for Python 2 and 3).

Testing `common` driver:
```sh
make NAME=led_blinker HOST=192.168.1.100 test_common
```
