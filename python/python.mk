
KOHERON_PYTHON_VERSION := $(shell cd python && $(PYTHON) -c "from koheron.version import __version__; print(__version__)")
PYPI_VERSION := $(shell curl -s 'https://pypi.org/pypi/koheron/json'| PYTHONIOENCODING=utf8 $(PYTHON) -c "import sys, json; print(json.load(sys.stdin)['info']['version'])")
PIP := pip$(shell $(PYTHON) -c "import sys; print(sys.version_info[0])")

PHONY: upload_pip
upload_pip:
	@echo $(KOHERON_PYTHON_VERSION)
	@echo $(PYPI_VERSION)
	rm -rf python/build
	rm -rf python/dist
	rm -rf python/koheron.egg-info
ifneq ($(PYPI_VERSION), $(KOHERON_PYTHON_VERSION))
	cd python && $(PYTHON) setup.py sdist bdist_wheel
	twine upload -u $(PYPI_USERNAME) -p $(PYPI_PASSWORD) python/dist/*
endif
