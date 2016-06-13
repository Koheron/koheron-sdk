###############################################################################
# Build the zip file: $ make NAME=spectrum HOST=192.168.1.12 zip
###############################################################################

NAME = oscillo
HOST = 192.168.1.100

###############################################################################
# Get the project configuration
# MAKE_PY script parses the properties defined MAIN_YML
###############################################################################

MAIN_YML = projects/$(NAME)/main.yml
MAKE_PY = scripts/make.py

# Store all build artifacts in TMP
TMP = tmp

# properties defined MAIN_YML :
BOARD:=$(shell python $(MAKE_PY) --board $(NAME) && cat $(TMP)/$(NAME).board)
CORES:=$(shell python $(MAKE_PY) --cores $(NAME) && cat $(TMP)/$(NAME).cores)
DRIVERS:=$(shell python $(MAKE_PY) --drivers $(NAME) && cat $(TMP)/$(NAME).drivers)
XDC:=$(shell python $(MAKE_PY) --xdc $(NAME) && cat $(TMP)/$(NAME).xdc)

PART:=`cat boards/$(BOARD)/PART`
PATCHES = boards/$(BOARD)/patches
PROC = ps7_cortexa9_0

# Custom commands
VIVADO_VERSION = 2016.1
VIVADO = vivado -nolog -nojournal -mode batch
HSI = hsi -nolog -nojournal -mode batch
RM = rm -rf

DOCKER=False

ifeq ($(DOCKER),False)
	PYTHON=$(TCP_SERVER_VENV)/bin/python
else
	PYTHON=/usr/bin/python
endif

###############################################################################
# Linux and U-boot
###############################################################################

# solves problem with awk while building linux kernel
# solution taken from http://www.googoolia.com/wp/2015/04/21/awk-symbol-lookup-error-awk-undefined-symbol-mpfr_z_sub/
LD_LIBRARY_PATH =

UBOOT_TAG = xilinx-v$(VIVADO_VERSION)
LINUX_TAG = xilinx-v$(VIVADO_VERSION)
DTREE_TAG = xilinx-v$(VIVADO_VERSION)

UBOOT_DIR = $(TMP)/u-boot-xlnx-$(UBOOT_TAG)
LINUX_DIR = $(TMP)/linux-xlnx-$(LINUX_TAG)
DTREE_DIR = $(TMP)/device-tree-xlnx-$(DTREE_TAG)

UBOOT_TAR = $(TMP)/u-boot-xlnx-$(UBOOT_TAG).tar.gz
LINUX_TAR = $(TMP)/linux-xlnx-$(LINUX_TAG).tar.gz
DTREE_TAR = $(TMP)/device-tree-xlnx-$(DTREE_TAG).tar.gz

UBOOT_URL = https://github.com/Xilinx/u-boot-xlnx/archive/$(UBOOT_TAG).tar.gz
LINUX_URL = https://github.com/Xilinx/linux-xlnx/archive/$(LINUX_TAG).tar.gz
DTREE_URL = https://github.com/Xilinx/device-tree-xlnx/archive/$(DTREE_TAG).tar.gz

LINUX_CFLAGS = "-O2 -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard"
UBOOT_CFLAGS = "-O2 -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard"
ARMHF_CFLAGS = "-O2 -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard"

RTL_TAR = $(TMP)/rtl8192cu.tgz
RTL_URL = https://googledrive.com/host/0B-t5klOOymMNfmJ0bFQzTVNXQ3RtWm5SQ2NGTE1hRUlTd3V2emdSNzN6d0pYamNILW83Wmc/rtl8192cu/rtl8192cu.tgz

# Project configuration

CONFIG_TCL = projects/$(NAME)/config.tcl
TEMPLATE_DIR = scripts/templates

# Versioning
VERSION_FILE = $(TMP)/$(NAME).version
VERSION = $(shell cat $(VERSION_FILE))
SHA_FILE = $(TMP)/$(NAME).sha
SHA = $(shell cat $(SHA_FILE))

# Zip
TCP_SERVER_DIR = $(TMP)/$(NAME).tcp-server
TCP_SERVER = $(TCP_SERVER_DIR)/tmp/kserverd
SERVER_CONFIG = projects/$(NAME)/drivers.yml
TCP_SERVER_SHA = master
TCP_SERVER_VENV = $(TMP)/$(NAME).tcp_server_venv
TCP_SERVER_MIDDLEWARE = $(TMP)/$(NAME).middleware

ZIP = $(TMP)/$(NAME)-$(VERSION).zip

# App
S3_URL = http://zynq-sdk.s3-website-eu-west-1.amazonaws.com
APP_SHA := $(shell curl -s $(S3_URL)/apps | cut -d" " -f1)
APP_URL = $(S3_URL)/app-$(APP_SHA).zip
APP_ZIP = $(TMP)/app.zip

.PRECIOUS: $(TMP)/cores/% $(TMP)/%.xpr $(TMP)/%.hwdef $(TMP)/%.bit $(TMP)/%.fsbl/executable.elf $(TMP)/%.tree/system.dts

all: zip boot.bin uImage devicetree.dtb fw_printenv tcp-server_cli app

$(TMP):
	mkdir -p $(TMP)

###############################################################################
# tests
###############################################################################

test_module: $(CONFIG_TCL) projects/$(NAME)/*.tcl $(addprefix $(TMP)/cores/, $(CORES))
	vivado -source scripts/test_module.tcl -tclargs $(NAME) $(PART)

test_core:
	vivado -source scripts/test_core.tcl -tclargs $(CORE) $(PART)

test: tests/$(NAME).py
	python $<

run: zip
	curl -v -F $(NAME)-$(VERSION).zip=@$(ZIP) http://$(HOST)/api/upload/instrument_zip	
	curl http://$(HOST)/api/deploy/local/$(NAME)-$(VERSION).zip

###############################################################################
# versioning
###############################################################################

$(VERSION_FILE): | $(TMP)
	echo $(shell (git rev-parse --short HEAD || echo "default")) > $@

$(SHA_FILE): $(VERSION_FILE)
	echo $(shell (printf $(NAME)-$(VERSION) | sha256sum | sed 's/\W//g')) > $@

###############################################################################
# FPGA
###############################################################################

$(CONFIG_TCL): $(MAKE_PY) $(MAIN_YML) $(SHA_FILE) $(TEMPLATE_DIR)/config.tcl
	python $(MAKE_PY) --config_tcl $(NAME)

$(TMP)/cores/%: fpga/cores/%/core_config.tcl fpga/cores/%/*.v
	mkdir -p $(@D)
	$(VIVADO) -source scripts/core.tcl -tclargs $* $(PART)

$(TMP)/%.xpr: $(CONFIG_TCL) $(XDC) projects/%/*.tcl $(addprefix $(TMP)/cores/, $(CORES))
	mkdir -p $(@D)
	$(VIVADO) -source scripts/project.tcl -tclargs $* $(PART) $(BOARD)

$(TMP)/%.bit: $(TMP)/%.xpr
	mkdir -p $(@D)
	$(VIVADO) -source scripts/bitstream.tcl -tclargs $*

###############################################################################
# first-stage boot loader
###############################################################################

$(TMP)/%.fsbl/executable.elf: $(TMP)/%.hwdef
	mkdir -p $(@D)
	$(HSI) -source scripts/fsbl.tcl -tclargs $* $(PROC)

###############################################################################
# U-Boot
###############################################################################

$(UBOOT_TAR):
	mkdir -p $(@D)
	curl -L $(UBOOT_URL) -o $@

$(UBOOT_DIR): $(UBOOT_TAR)
	mkdir -p $@
	tar -zxf $< --strip-components=1 --directory=$@
	patch -d $(TMP) -p 0 < $(PATCHES)/u-boot-xlnx-$(UBOOT_TAG).patch
	bash $(PATCHES)/uboot.sh $(PATCHES) $@

$(TMP)/u-boot.elf: $(UBOOT_DIR)
	mkdir -p $(@D)
	make -C $< mrproper
	make -C $< arch=arm `find $(PATCHES) -name '*_defconfig' -exec basename {} \;`
	make -C $< arch=arm CFLAGS=$(UBOOT_CFLAGS) \
	  CROSS_COMPILE=arm-linux-gnueabihf- all
	cp $</u-boot $@

fw_printenv: $(UBOOT_DIR) $(TMP)/u-boot.elf
	make -C $< arch=ARM CFLAGS=$(ARMHF_CFLAGS) \
	  CROSS_COMPILE=arm-linux-gnueabihf- env
	cp $</tools/env/fw_printenv $@

###############################################################################
# boot.bin
###############################################################################

boot.bin: $(TMP)/$(NAME).fsbl/executable.elf $(TMP)/$(NAME).bit $(TMP)/u-boot.elf
	echo "img:{[bootloader] $^}" > $(TMP)/boot.bif
	bootgen -image $(TMP)/boot.bif -w -o i $@

###############################################################################
# device tree
###############################################################################

$(TMP)/%.hwdef: $(TMP)/%.xpr
	mkdir -p $(@D)
	$(VIVADO) -source scripts/hwdef.tcl -tclargs $*

$(DTREE_TAR):
	mkdir -p $(@D)
	curl -L $(DTREE_URL) -o $@

$(DTREE_DIR): $(DTREE_TAR)
	mkdir -p $@
	tar -zxf $< --strip-components=1 --directory=$@

$(TMP)/%.tree/system.dts: $(TMP)/%.hwdef $(DTREE_DIR)
	mkdir -p $(@D)
	$(HSI) -source scripts/devicetree.tcl -tclargs $* $(PROC) $(DTREE_DIR) $(VIVADO_VERSION)
	patch $@ $(PATCHES)/devicetree.patch

###############################################################################
# Linux
###############################################################################

$(RTL_TAR):
	mkdir -p $(@D)
	curl -L $(RTL_URL) -o $@

$(LINUX_TAR):
	mkdir -p $(@D)
	curl -L $(LINUX_URL) -o $@

$(LINUX_DIR): $(LINUX_TAR) $(RTL_TAR)
	mkdir -p $@
	tar -zxf $< --strip-components=1 --directory=$@
	tar -zxf $(RTL_TAR) --directory=$@/drivers/net/wireless/realtek
	patch -d $(TMP) -p 0 < $(PATCHES)/linux-xlnx-$(LINUX_TAG).patch
	bash $(PATCHES)/linux.sh $(PATCHES) $@

uImage: $(LINUX_DIR)
	make -C $< mrproper
	make -C $< ARCH=arm xilinx_zynq_defconfig
	make -C $< ARCH=arm CFLAGS=$(LINUX_CFLAGS) \
	  -j $(shell nproc 2> /dev/null || echo 1) \
	  CROSS_COMPILE=arm-linux-gnueabihf- UIMAGE_LOADADDR=0x8000 uImage
	cp $</arch/arm/boot/uImage $@

devicetree.dtb: uImage $(TMP)/$(NAME).tree/system.dts
	$(LINUX_DIR)/scripts/dtc/dtc -I dts -O dtb -o devicetree.dtb \
	  -i $(TMP)/$(NAME).tree $(TMP)/$(NAME).tree/system.dts

###############################################################################
# tcp-server (compiled with project specific middleware)
###############################################################################

$(TCP_SERVER_DIR): 
	git clone https://github.com/Koheron/tcp-server.git $(TCP_SERVER_DIR)
	cd $(TCP_SERVER_DIR) && git checkout $(TCP_SERVER_SHA)
	echo `cd $(TCP_SERVER_DIR) && git rev-parse HEAD` > $(TCP_SERVER_DIR)/VERSION

$(TCP_SERVER_DIR)/requirements.txt: $(TCP_SERVER_DIR)

$(TCP_SERVER_VENV): $(TCP_SERVER_DIR)/requirements.txt
ifeq ($(DOCKER),False)
	virtualenv $(TCP_SERVER_VENV)
	$(TCP_SERVER_VENV)/bin/pip install -r $(TCP_SERVER_DIR)/requirements.txt
else
	/usr/bin/pip install -r $(TCP_SERVER_DIR)/requirements.txt
endif

$(TCP_SERVER_MIDDLEWARE)/%: %
	mkdir -p -- `dirname -- $@`
	cp $^ $@

$(TCP_SERVER_MIDDLEWARE): $(addprefix $(TCP_SERVER_MIDDLEWARE)/, $(DRIVERS)) drivers/lib
	python $(MAKE_PY) --middleware $(NAME)
	cp -R drivers/lib $(TCP_SERVER_MIDDLEWARE)/drivers/

$(TCP_SERVER): $(TCP_SERVER_VENV) $(MAKE_PY) $(SERVER_CONFIG) $(TCP_SERVER_MIDDLEWARE)
	cd $(TCP_SERVER_DIR) && make CONFIG=$(SERVER_CONFIG) BASE_DIR=../.. PYTHON=$(PYTHON) MIDWARE_PATH=$(TCP_SERVER_MIDDLEWARE)

tcp-server_cli: $(TCP_SERVER_DIR) $(TCP_SERVER_VENV)
	cd $(TCP_SERVER_DIR) && make CONFIG=$(SERVER_CONFIG) BASE_DIR=../.. PYTHON=$(PYTHON) cli

###############################################################################
# zip (contains bitstream, tcp-server)
###############################################################################

zip: $(TCP_SERVER) $(VERSION_FILE) $(PYTHON_DIR) $(TMP)/$(NAME).bit
	zip --junk-paths $(ZIP) $(TMP)/$(NAME).bit $(TCP_SERVER)

###############################################################################
# app
###############################################################################

app: $(TMP)
	echo $(APP_SHA)
	curl -L $(APP_URL) -o $(APP_ZIP)

###############################################################################
# clean
###############################################################################

clean:
	$(RM) uImage fw_printenv boot.bin devicetree.dtb $(TMP)
	$(RM) .Xil usage_statistics_webtalk.html usage_statistics_webtalk.xml
	$(RM) webtalk*.log webtalk*.jou
