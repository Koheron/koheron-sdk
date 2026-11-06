# Inspection and host commands. Recipes use the fully resolved build settings.
.PHONY: help
help:
	@echo ' - all          : (Default goal) build the instrument: fpga, server and web'
	@echo ' - run          : Run the instrument'
	@echo ' - deploy       : Build, upload and wait for readiness, then return (TIMEOUT=120)'
	@echo ' - status       : Read board and instrument status (HOST=..., TIMEOUT=10)'
	@echo ' - logs         : Read recent server logs (LOG_LIMIT=100, TIMEOUT=10)'
	@echo ' - info         : Resolved CFG, board, toolchains and artifact paths; FORMAT=json supported'
	@echo ' - check        : Validate CFG (or all examples) and run local host regressions'
	@echo ' - fpga         : Build the FPGA bitstream'
	@echo '                  FPGA_SYNTH_MODE=ip enables experimental per-IP synthesis caching'
	@echo ' - timing       : Check routed FPGA timing (fails on violations)'
	@echo '                  Set ENFORCE_TIMING := 1 in an example config.mk to enforce during builds'
	@echo ' - server       : Build the server'
	@echo ' - web          : Build the web interface'
	@echo ' - os           : Build the operating system'
	@echo ' - uboot        : Build the board bootloader independently of FPGA and Linux'
	@echo ' - image        : Build the full image'
	@echo '                  Set PASSWORD in the environment to customize the image root password'
	@echo ' - flash        : Flash the existing release ZIP (run image first)'
	@echo ' - block_design : Build the Vivado block design interactively'
	@echo ' - open_project : Open the Vivado .xpr project'
	@echo ' - doctor       : Check requirements for TARGET=all|web|server|fpga|image|os|uboot|api'
	@echo '                  Or TARGET=check; FORMAT=json supported'
	@echo ' - validate     : Validate selected CFG and memory.yml'
	@echo ' - copy         : Copy selected CFG instrument to DEST and set its NAME'
	@echo ' - list         : List available example instruments'
	@echo ' - examples     : Alias for list'

.PHONY: clean_all
clean_all:
	$(HOST_PYTHON) $(SDK_PATH)/python/koheron/safe_clean.py --sdk-path $(call build_shell_quote,$(SDK_FULL_PATH)) --root $(call build_shell_quote,$(TMP)) --all

SDK_INFO_VARS := SDK_PATH CONFIG_MK NAME VERSION BOARD BOARD_PATH BOARD_MK PART ZYNQ_TYPE UBUNTU_ARCH \
  MEMORY_YML BD_TCL DRIVERS XDC WEB_FILES VIVADO_VERSION VIVADO_PATH VITIS_PATH \
  KOHERON_VERSION GCC_ARCH GCC_VERSION KERNEL_CC UBOOT_CC OS_HOSTCC \
  LINUX_VERSION UBOOT_VERSION UBUNTU_VERSION DOCKER_IMAGE WEB_DOCKER_IMAGE \
  MODE FPGA_SYNTH_MODE ENFORCE_TIMING N_CPUS INSTRUMENT_ZIP BOARD_INSTRUMENT_ZIP \
  BITSTREAM SERVER DRIVERS_JSON_OUT RELEASE_ZIP MANIFEST_TXT INSTRUMENT_TEST FSBL_STDIO
SDK_INFO_ARGS = $(foreach K,$(SDK_INFO_VARS),$(call build_shell_quote,$(K)=$($(K))))
SDK_INFO_COMMAND = $(HOST_PYTHON) $(call build_shell_quote,$(SDK_PATH)/python/koheron/sdk_info.py)
SDK_MANAGEMENT_COMMAND = $(HOST_PYTHON) $(call build_shell_quote,$(SDK_PATH)/python/koheron/management.py) \
  --host $(call build_shell_quote,$(HOST)) --timeout $(call build_shell_quote,$(TIMEOUT)) \
  --format $(call build_shell_quote,$(FORMAT))

.PHONY: info
info:
	@$(SDK_INFO_COMMAND) --format $(call build_shell_quote,$(FORMAT)) $(SDK_INFO_ARGS)

.PHONY: check
check:
	@$(if $(CFG),$(SDK_INFO_COMMAND) --format json $(SDK_INFO_ARGS) |,) $(HOST_PYTHON) $(call build_shell_quote,$(SDK_PATH)/python/koheron/check.py) \
		--sdk-path $(call build_shell_quote,$(SDK_PATH)) --format $(call build_shell_quote,$(FORMAT)) $(if $(CFG),--settings-stdin,)

.PHONY: status logs
status:
	@$(SDK_MANAGEMENT_COMMAND) status

logs:
	@$(SDK_MANAGEMENT_COMMAND) logs --limit $(call build_shell_quote,$(LOG_LIMIT))

.PHONY: list examples
list:
	@$(HOST_PYTHON) "$(SDK_PATH)/python/koheron/list_examples.py" --sdk-path "$(SDK_PATH)"

examples: list

.PHONY: copy
copy:
	@python3 "$(SDK_PATH)/python/koheron/copy_instrument.py" \
		--sdk-path "$(SDK_PATH)" --cfg "$(CFG)" --destination "$(DEST)"

.PHONY: doctor
doctor:
	@$(if $(CFG),$(SDK_INFO_COMMAND) --format json $(SDK_INFO_ARGS) |,) $(HOST_PYTHON) "$(SDK_PATH)/python/koheron/doctor.py" \
		--sdk-path "$(SDK_PATH)" \
		--target $(call build_shell_quote,$(TARGET)) --format $(call build_shell_quote,$(FORMAT)) \
		--python $(call build_shell_quote,$(HOST_PYTHON)) \
		--docker-image $(call build_shell_quote,$(DOCKER_IMAGE)) --web-image $(call build_shell_quote,$(WEB_DOCKER_IMAGE)) \
		--xilinx-version "$(VIVADO_VERSION)" \
		--vivado-path "$(VIVADO_PATH)" \
		--vitis-path "$(VITIS_PATH)" $(if $(CFG),--settings-stdin,)

.PHONY: validate
validate: $(PYTHON_REQUIREMENTS_STAMP)
	@$(SDK_INFO_COMMAND) --format json $(SDK_INFO_ARGS) | \
		"$(VENV)/bin/python$(PYTHON_VERSION)" "$(SDK_PATH)/python/koheron/validate_config.py" \
		--sdk-path "$(SDK_PATH)" \
		--settings-stdin
