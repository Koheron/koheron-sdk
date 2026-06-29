KOHERON_PYTHON_VERSION = $(shell cd python && python3 -c "from koheron.version import __version__; print(__version__)" 2>/dev/null)
PYPI_VERSION_CMD = curl -fsS 'https://pypi.org/pypi/koheron/json' 2>/dev/null | PYTHONIOENCODING=utf8 python3 -c "import sys, json; print(json.load(sys.stdin)['info']['version'])" 2>/dev/null || true
PIP := $(VENV)/bin/pip

.PHONY: upload_pip
upload_pip: $(KOHERON_PYTHON_STAMP)
	@koheron_version='$(KOHERON_PYTHON_VERSION)'; \
	pypi_version="$$( $(PYPI_VERSION_CMD) )"; \
	echo "$$koheron_version"; \
	echo "$$pypi_version"; \
	rm -rf python/build python/dist python/koheron.egg-info; \
	if [ "$$pypi_version" != "$$koheron_version" ]; then \
		cd python && $(VENV)/bin/python3 setup.py sdist bdist_wheel; \
		cd .. && twine upload -u $(PYPI_USERNAME) -p $(PYPI_PASSWORD) python/dist/*; \
	fi
