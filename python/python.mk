
KOHERON_PYTHON_VERSION := $(shell cd python && python3 -c "from koheron.version import __version__; print(__version__)")
PYPI_VERSION := $(shell curl -s 'https://pypi.org/pypi/koheron/json'| PYTHONIOENCODING=utf8 python3 -c "import sys, json; print(json.load(sys.stdin)['info']['version'])")
PIP := $(VENV)/bin/pip

.PHONY: upload_pip
upload_pip:
	@echo $(KOHERON_PYTHON_VERSION)
	@echo $(PYPI_VERSION)
	rm -rf python/build
	rm -rf python/dist
	rm -rf python/koheron.egg-info
ifneq ($(PYPI_VERSION), $(KOHERON_PYTHON_VERSION))
	cd python && $(VENV)/bin/python3 setup.py sdist bdist_wheel
	twine upload -u $(PYPI_USERNAME) -p $(PYPI_PASSWORD) python/dist/*
endif
