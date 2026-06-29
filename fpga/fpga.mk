FPGA_PATH := $(FPGA_PATH)
TMP_FPGA_PATH := $(TMP_PROJECT_PATH)/fpga

$(TMP_FPGA_PATH):
	@mkdir -p $@

VIVADO_AWK = awk '\
  BEGIN { \
    RED="\033[31;1m"; \
    YELLOW="\033[33;1m"; \
    GREEN="\033[32;1m"; \
    MAGENTA="\033[35;1m"; \
    CYAN="\033[96;1m"; \
    RESET="\033[0m"; \
  } \
  /^\#/               { $$0 = CYAN $$0 RESET } \
  /ERROR:/            { sub(/ERROR:/, RED "&" RESET) } \
  /CRITICAL WARNING:/ { sub(/CRITICAL WARNING:/, MAGENTA "&" RESET) } \
  /WARNING:/          { sub(/WARNING:/, YELLOW "&" RESET) } \
  /INFO:/             { sub(/INFO:/, GREEN "&" RESET) } \
  { print } \
'

VIVADO := source $(VIVADO_PATH)/settings64.sh && vivado -nolog -nojournal -notrace
VIVADO_BATCH := $(VIVADO) -mode batch

# Cores
###############################################################################
TMP_CORES_PATH := $(TMP_PROJECT_PATH)/cores
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

MEMORY_TCL := $(TMP_FPGA_PATH)/memory.tcl

$(MEMORY_TCL): $(MEMORY_YML) $(FPGA_PATH)/memory.tcl
	$(MAKE_PY) --memory_tcl $@ $(MEMORY_YML)
	$(call ok,$@)

.PHONY: xpr
xpr: $(TMP_FPGA_PATH)/$(NAME).xpr.stamp

export SDK_PATH
export NAME
export PROJECT_PATH
export PART
export BOARD_PATH
export MODE
export TMP_FPGA_PATH
export TMP_CORES_PATH
export XDC
export VENV
export BD_TCL

$(TMP_FPGA_PATH)/$(NAME).xpr.stamp: $(MEMORY_TCL) $(TCL_FILES) $(CORES_COMPONENT_XML) $(XDC) | $(TMP_FPGA_PATH)/
	$(VIVADO_BATCH) -source $(FPGA_PATH)/vivado/project.tcl 2>&1 | $(VIVADO_AWK)
	touch $@
	$(call ok,$@)

.PHONY: xsa
xsa: $(TMP_FPGA_PATH)/$(NAME).xsa

$(TMP_FPGA_PATH)/$(NAME).xsa: $(TMP_FPGA_PATH)/$(NAME).xpr.stamp | $(TMP_FPGA_PATH)/
	$(VIVADO_BATCH) -source $(FPGA_PATH)/vivado/hwdef.tcl -tclargs $(TMP_FPGA_PATH)/$(NAME).xpr $@ $(N_CPUS) 2>&1 | $(VIVADO_AWK)
	$(call ok,$@)

.PHONY: fpga
fpga: $(BITSTREAM)

$(BITSTREAM): $(TMP_FPGA_PATH)/$(NAME).xsa | $(TMP_FPGA_PATH)/
	$(VIVADO_BATCH) -source $(FPGA_PATH)/vivado/bitstream.tcl -tclargs $(TMP_FPGA_PATH)/$(NAME).xpr $@ $(ZYNQ_TYPE) $(N_CPUS) 2>&1 | $(VIVADO_AWK)
	$(call ok,$@)

$(BITSTREAM).bin: $(BITSTREAM)
	echo "all:{$(BITSTREAM)}" > $(TMP_FPGA_PATH)/overlay.bif
	$(BOOTGEN) -image $(TMP_FPGA_PATH)/overlay.bif -arch $(ZYNQ_TYPE) -process_bitstream bin -w on -o $(BITSTREAM).bin
	$(call ok,$@)

# Build the block design in Vivado GUI
.PHONY: block_design
block_design: $(MEMORY_TCL) $(TCL_FILES) $(CORES_COMPONENT_XML) $(XDC_FILE)
	$(VIVADO) -source $(FPGA_PATH)/vivado/block_design.tcl \
	  -tclargs block_design_ 2>&1 | $(VIVADO_AWK)

# Open the Vivado project
.PHONY: open_project
open_project: $(TMP_FPGA_PATH)/$(NAME).xpr
	$(VIVADO) -source $(FPGA_PATH)/vivado/open_project.tcl -tclargs $(TMP_FPGA_PATH)/$(NAME).xpr

# Build and test a module in Vivado GUI
.PHONY: test_module
test_module: $(MEMORY_TCL) $(PROJECT_PATH)/*.tcl $(CORES_COMPONENT_XML)
	$(VIVADO) -source $(FPGA_PATH)/vivado/test_module.tcl 2>&1 | $(VIVADO_AWK)

# Build and test a core in Vivado GUI
CORE ?= $(FPGA_PATH)/cores/pdm_v1_0
.PHONY: test_core
test_core: $(CORE)/core_config.tcl $(wildcard $(CORE)/*.v $(CORE)/*.sv $(CORE)/*.vh $(CORE)/*.vhd $(CORE)/*.vhdl)
	$(VIVADO) -source $(FPGA_PATH)/vivado/test_core.tcl -tclargs $(CORE) 2>&1 | $(VIVADO_AWK)

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
