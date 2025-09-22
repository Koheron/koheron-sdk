FPGA_PATH := $(FPGA_PATH)
TMP_FPGA_PATH := $(TMP_PROJECT_PATH)/fpga

$(TMP_FPGA_PATH):
	@mkdir -p $@

VIVADO := source $(VIVADO_PATH)/settings64.sh && vivado -nolog -nojournal -notrace
VIVADO_BATCH := $(VIVADO) -mode batch

$(MEMORY_YML): $(CONFIG_YML) | $(TMP_PROJECT_PATH)/
	$(MAKE_PY) --memory_yml $< $@
	$(call ok,$@)

# Cores
###############################################################################
TMP_CORES_PATH := $(TMP)/cores
$(TMP_CORES_PATH)/: ; @mkdir -p $@

define make_core_target
$(TMP_CORES_PATH)/$(notdir $1)/component.xml: \
    $(wildcard $1/*.v $1/*.sv $1/*.vh $1/*.vhd $1/*.vhdl) $1/core_config.tcl | $(TMP_CORES_PATH)/
	$(VIVADO_BATCH) -source $(FPGA_PATH)/vivado/core.tcl -tclargs $1 $(PART) $(TMP_CORES_PATH)
	$(call ok,$$@)
endef
$(foreach core,$(CORES),$(eval $(call make_core_target,$(core))))

CORES_COMPONENT_XML := $(addsuffix /component.xml,$(addprefix $(TMP_CORES_PATH)/,$(notdir $(CORES))))

.PHONY: cores
cores: $(CORES_COMPONENT_XML)
	$(call ok,$@)

# Vivado project
###############################################################################

XDC_FILE := $(TMP_FPGA_PATH)/xdc
$(XDC_FILE): $(XDC) | $(TMP_FPGA_PATH)/
	@{ \
	  for f in $(XDC); do printf "%s " "$$f"; done; \
	} > $@
	@echo "[constraints] wrote list to $@"

CONFIG_TCL := $(TMP_FPGA_PATH)/config.tcl
$(CONFIG_TCL): $(MEMORY_YML) $(FPGA_PATH)/config.tcl
	$(MAKE_PY) --config_tcl $(CONFIG_YML) $@
	$(call ok,$@)

.PHONY: xpr
xpr: $(TMP_FPGA_PATH)/$(NAME).xpr

$(TMP_FPGA_PATH)/$(NAME).xpr: $(CONFIG_TCL) $(PROJECT_PATH)/*.tcl $(CORES_COMPONENT_XML) $(XDC_FILE) | $(TMP_FPGA_PATH)/
	$(VIVADO_BATCH) -source $(FPGA_PATH)/vivado/project.tcl \
	  -tclargs $(SDK_PATH) $(NAME) $(PROJECT_PATH) $(PART) $(BOARD_PATH) $(MODE) $(TMP_FPGA_PATH) $(XDC_FILE) $(VENV)/bin/$(PYTHON)
	$(call ok,$@)

.PHONY: fpga
fpga: $(BITSTREAM)

$(BITSTREAM): $(TMP_FPGA_PATH)/$(NAME).xpr | $(TMP_FPGA_PATH)/
	$(VIVADO_BATCH) -source $(FPGA_PATH)/vivado/bitstream.tcl -tclargs $< $@ $(ZYNQ_TYPE) $(N_CPUS)
	$(call ok,$@)

$(BITSTREAM).bin: $(BITSTREAM)
	echo "all:{$(BITSTREAM)}" > $(TMP_FPGA_PATH)/overlay.bif
	$(BOOTGEN) -image $(TMP_FPGA_PATH)/overlay.bif -arch $(ZYNQ_TYPE) -process_bitstream bin -w on -o $(BITSTREAM).bin
	$(call ok,$@)

$(TMP_FPGA_PATH)/$(NAME).xsa: $(TMP_FPGA_PATH)/$(NAME).xpr | $(TMP_FPGA_PATH)/
	$(VIVADO_BATCH) -source $(FPGA_PATH)/vivado/hwdef.tcl -tclargs $(TMP_FPGA_PATH)/$(NAME).xpr $(TMP_FPGA_PATH)/$(NAME).xsa $(N_CPUS)
	$(call ok,$@)

# Build the block design in Vivado GUI
.PHONY: block_design
block_design: $(CONFIG_TCL) $(PROJECT_PATH)/*.tcl $(CORES_COMPONENT_XML) $(XDC_FILE)
	$(VIVADO) -source $(FPGA_PATH)/vivado/block_design.tcl \
	  -tclargs $(SDK_PATH) $(NAME) $(PROJECT_PATH) $(PART) $(BOARD_PATH) $(MODE) $(TMP_FPGA_PATH) $(XDC_FILE) $(VENV)/bin/$(PYTHON) block_design_

# Open the Vivado project
.PHONY: open_project
open_project: $(TMP_FPGA_PATH)/$(NAME).xpr
	$(VIVADO) -source $(FPGA_PATH)/vivado/open_project.tcl -tclargs $(TMP_FPGA_PATH)/$(NAME).xpr

# Build and test a module in Vivado GUI
.PHONY: test_module
test_module: $(CONFIG_TCL) $(PROJECT_PATH)/*.tcl $(CORES_COMPONENT_XML)
	$(VIVADO) -source $(FPGA_PATH)/vivado/test_module.tcl -tclargs $(SDK_PATH) $(NAME) $(PROJECT_PATH) $(PART) $(TMP_FPGA_PATH)

# Build and test a core in Vivado GUI
CORE ?= $(FPGA_PATH)/cores/pdm_v1_0
.PHONY: test_core
test_core: $(CORE)/core_config.tcl $(wildcard $(CORE)/*.v $(CORE)/*.sv $(CORE)/*.vh $(CORE)/*.vhd $(CORE)/*.vhdl)
	$(VIVADO) -source $(FPGA_PATH)/vivado/test_core.tcl -tclargs $(CORE) $(PART) $(TMP_CORES_PATH)

# Clean targets
###############################################################################
.PHONY: clean_fpga
clean_fpga:
	rm -rf $(TMP_FPGA_PATH)

.PHONY: clean_cores
clean_cores:
	rm -rf $(TMP_CORES_PATH)

.PHONY: clean_core
clean_core:
	rm -rf $(TMP_CORES_PATH)/$(notdir $(CORE))*