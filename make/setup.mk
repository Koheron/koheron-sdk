# Host installation and Python package stamps. Included for all invocations.
PYTHON_PACKAGE_FILES := $(shell find $(SDK_PATH)/python -type f \
	\( -name '*.py' -o -name 'setup.py' -o -name 'pyproject.toml' -o -name 'setup.cfg' \) 2>/dev/null)

$(PYTHON_REQUIREMENTS_STAMP): $(SDK_PATH)/requirements.txt
	@mkdir -p $(@D)
	@[ -x $(VENV)/bin/python$(PYTHON_VERSION) ] || python$(PYTHON_VERSION) -m venv $(VENV)
	@$(VENV)/bin/python$(PYTHON_VERSION) -m ensurepip --upgrade >/dev/null
	@$(VENV)/bin/python$(PYTHON_VERSION) -m pip install --upgrade pip
	@$(VENV)/bin/python$(PYTHON_VERSION) -m pip install -r $(SDK_PATH)/requirements.txt
	@touch $@
	$(call ok,python requirements)

$(KOHERON_PYTHON_STAMP): $(PYTHON_REQUIREMENTS_STAMP) $(PYTHON_PACKAGE_FILES)
	@$(VENV)/bin/python$(PYTHON_VERSION) -m pip install $(SDK_PATH)/python
	@touch $@
	$(call ok,koheron python package)

.PHONY: python_requirements koheron_python
python_requirements: $(PYTHON_REQUIREMENTS_STAMP)
koheron_python: $(KOHERON_PYTHON_STAMP)

DISTRO := $(shell bash ./.setup/get_distro.sh)
.PHONY: setup
setup:
	sudo bash .setup/install_dependencies_$(DISTRO).sh
	$(MAKE) --no-print-directory $(KOHERON_PYTHON_STAMP)
	bash docker/install_docker_$(DISTRO).sh
	sudo usermod -aG docker $(shell whoami)
	@if ! docker info >/dev/null 2>&1; then \
		sudo setfacl -m u:$(shell id -u):rw /var/run/docker.sock; \
	fi
	docker build -f $(DOCKER_PATH)/Dockerfile --build-arg UBUNTU_VERSION=$(DOCKER_UBUNTU_VERSION) --build-arg GCC_VERSION=$(GCC_VERSION) -t $(DOCKER_IMAGE) $(DOCKER_PATH)
	docker build -f $(WEB_PATH)/Dockerfile.web -t $(WEB_DOCKER_IMAGE) $(WEB_PATH)
	@echo 'Setup complete.'
