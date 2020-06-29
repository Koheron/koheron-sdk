FPGA_PATH := $(FPGA_PATH)
TMP_FPGA_PATH := $(TMP_PROJECT_PATH)/fpga

$(TMP_FPGA_PATH):
	@mkdir -p $@

BOARD_PATH := $(shell $(MAKE_PY) --board $(CONFIG) $(TMP_FPGA_PATH)/board && cat $(TMP_FPGA_PATH)/board)
PART := $(shell cat $(BOARD_PATH)/PART)

VIVADO := source $(VIVADO_PATH)/$(VIVADO_VERSION)/settings64.sh && vivado -nolog -nojournal
VIVADO_BATCH := $(VIVADO) -mode batch

$(MEMORY_YML): $(CONFIG)
	$(MAKE_PY) --memory_yml $(CONFIG) $@

include $(OS_PATH)/toolchain.mk

# Cores
###############################################################################
TMP_CORES_PATH := $(TMP_PROJECT_PATH)/cores

$(TMP_CORES_PATH):
	@mkdir -p $@

CORES := $(shell $(MAKE_PY) --cores $(CONFIG) $(TMP_CORES_PATH)/core_list && cat $(TMP_CORES_PATH)/core_list)
CORES_COMPONENT_XML := $(addsuffix /component.xml, $(addprefix $(TMP_CORES_PATH)/, $(notdir $(CORES))))

define make_core_target
$(TMP_CORES_PATH)/$(notdir $1)/component.xml: $1/core_config.tcl $1/*.v* | $(TMP_CORES_PATH)
	$(VIVADO_BATCH) -source $(FPGA_PATH)/vivado/core.tcl -tclargs $1 $(PART) $(TMP_CORES_PATH)
	@echo [$1] OK
endef
$(foreach core,$(CORES),$(eval $(call make_core_target,$(core))))

# Vivado project
###############################################################################

XDC := $(shell $(MAKE_PY) --xdc $(CONFIG) $(TMP_FPGA_PATH)/xdc && cat $(TMP_FPGA_PATH)/xdc)

CONFIG_TCL := $(TMP_FPGA_PATH)/config.tcl

$(CONFIG_TCL): $(MEMORY_YML) $(FPGA_PATH)/config.tcl
	$(MAKE_PY) --config_tcl $(CONFIG) $@
	@echo [$@] OK

.PHONY: xpr
xpr: $(TMP_FPGA_PATH)/$(NAME).xpr

$(TMP_FPGA_PATH)/$(NAME).xpr: $(CONFIG_TCL) $(XDC) $(PROJECT_PATH)/*.tcl $(CORES_COMPONENT_XML) | $(TMP_FPGA_PATH)
	$(VIVADO_BATCH) -source $(FPGA_PATH)/vivado/project.tcl \
	  -tclargs $(SDK_PATH) $(NAME) $(PROJECT_PATH) $(PART) $(BOARD_PATH) $(MODE) $(TMP_FPGA_PATH) $(TMP_FPGA_PATH)/xdc
	@echo [$@] OK

.PHONY: fpga
fpga: $(BITSTREAM)

$(BITSTREAM): $(TMP_FPGA_PATH)/$(NAME).xpr | $(TMP_FPGA_PATH)
	$(VIVADO_BATCH) -source $(FPGA_PATH)/vivado/bitstream.tcl -tclargs $< $@ $(ZYNQ_TYPE) $(N_CPUS)
	@echo [$@] OK

$(BITSTREAM).bin: $(BITSTREAM)
	echo "all:{$(BITSTREAM)}" > $(TMP_FPGA_PATH)/overlay.bif
	$(BOOTGEN) -image $(TMP_FPGA_PATH)/overlay.bif -arch $(ZYNQ_TYPE) -process_bitstream bin -w on -o $(BITSTREAM).bin 
	@echo [$@] OK

$(TMP_FPGA_PATH)/$(NAME).xsa: $(TMP_FPGA_PATH)/$(NAME).xpr | $(TMP_FPGA_PATH)
	$(VIVADO_BATCH) -source $(FPGA_PATH)/vivado/hwdef.tcl -tclargs $(TMP_FPGA_PATH)/$(NAME).xpr $(TMP_FPGA_PATH)/$(NAME).xsa $(N_CPUS)

	@echo [$@] OK

# Build the block design in Vivado GUI
.PHONY: block_design
block_design: $(CONFIG_TCL) $(XDC) $(PROJECT_PATH)/*.tcl $(CORES_COMPONENT_XML)
	$(VIVADO) -source $(FPGA_PATH)/vivado/block_design.tcl \
	  -tclargs $(SDK_PATH) $(NAME) $(PROJECT_PATH) $(PART) $(BOARD_PATH) $(MODE) $(TMP_FPGA_PATH) $(TMP_FPGA_PATH)/xdc block_design_

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
test_core: $(CORE)/core_config.tcl $(CORE)/*.v*
	$(VIVADO) -source $(FPGA_PATH)/vivado/test_core.tcl -tclargs $(CORE) $(PART) $(TMP_FPGA_PATH)

# Clean targets
###############################################################################

.PHONY: clean_fpga
clean_fpga:
	rm -rf $(TMP_FPGA_PATH)

.PHONY: clean_cores
clean_cores:
	rm -rf $(TMP_PROJECT_PATH)/cores

.PHONY: clean_core
clean_core:
	rm -rf $(TMP_PROJECT_PATH)/cores/$(CORE)*
