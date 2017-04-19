
KOHERON_PYTHON_VERSION := $(shell python -c "from python.version import __version__; print(__version__)")
PYPI_VERSION := $(shell curl -s 'https://pypi.python.org/pypi/koheron/json'| PYTHONIOENCODING=utf8 python -c "import sys, json; print json.load(sys.stdin)['info']['version']")

PHONY: upload_pip
upload_pip:
	@echo $(KOHERON_PYTHON_VERSION)
	@echo $(PYPI_VERSION)
	rm -rf build
	rm -rf dist
	rm -rf python/koheron.egg-info
ifneq ($(PYPI_VERSION), $(KOHERON_PYTHON_VERSION))
	python setup.py sdist bdist_wheel
	twine upload -u $(PYPI_USERNAME) -p $(PYPI_PASSWORD) dist/*
endif
