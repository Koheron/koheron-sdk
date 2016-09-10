###############################################################################
# Build and run the instrument: $ make NAME=spectrum HOST=192.168.1.12 run
###############################################################################

PROJECT_PATH = projects
NAME = led_blinker
HOST = 192.168.1.100

###############################################################################
# Get the project configuration
# MAKE_PY script parses the properties defined MAIN_YML
###############################################################################

MAIN_YML = $(TMP)/$(NAME).main.yml

MAKE_PY = scripts/make.py

# Store all build artifacts in TMP
TMP = tmp

DUMMY:=$(shell set -e; python $(MAKE_PY) --split_config_yml $(NAME) $(PROJECT_PATH))

# properties defined in MAIN_YML :
BOARD:=$(shell set -e; python $(MAKE_PY) --board $(NAME) $(PROJECT_PATH) && cat $(TMP)/$(NAME).board)
CORES:=$(shell set -e; python $(MAKE_PY) --cores $(NAME) $(PROJECT_PATH) && cat $(TMP)/$(NAME).cores)
DRIVERS:=$(shell set -e; python $(MAKE_PY) --drivers $(NAME) $(PROJECT_PATH) && cat $(TMP)/$(NAME).drivers)
XDC:=$(shell set -e; python $(MAKE_PY) --xdc $(NAME) $(PROJECT_PATH) && cat $(TMP)/$(NAME).xdc)
DRIVERS_LIB=$(wildcard drivers/lib/*hpp) $(wildcard drivers/lib/*cpp)

PART:=`cat boards/$(BOARD)/PART`
PATCHES = boards/$(BOARD)/patches
PROC = ps7_cortexa9_0

# Custom commands
VIVADO_VERSION = 2016.2
VIVADO = vivado -nolog -nojournal -mode batch
HSI = hsi -nolog -nojournal -mode batch
RM = rm -rf

DOCKER ?= False
ifeq ($(DOCKER),True)
	PYTHON=/usr/bin/python
else
	PYTHON=$(TCP_SERVER_VENV)/bin/python
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

LINUX_CFLAGS = "-O2 -march=armv7-a -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard"
UBOOT_CFLAGS = "-O2 -march=armv7-a -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard"
ARMHF_CFLAGS = "-O2 -march=armv7-a -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard"

RTL_TAR = $(TMP)/rtl8192cu.tgz
RTL_URL = https://googledrive.com/host/0B-t5klOOymMNfmJ0bFQzTVNXQ3RtWm5SQ2NGTE1hRUlTd3V2emdSNzN6d0pYamNILW83Wmc/rtl8192cu/rtl8192cu.tgz

# Project configuration
CONFIG_TCL = $(TMP)/$(NAME).config.tcl
TEMPLATE_DIR = scripts/templates

# Versioning
VERSION_FILE = $(TMP)/$(NAME).version
VERSION = $(shell (git rev-parse --short HEAD))
SHA_FILE = $(TMP)/$(NAME).sha
SHA = $(shell (printf $(NAME)-$(VERSION) | sha256sum | sed 's/\W//g'))

# Zip
TCP_SERVER_URL = https://github.com/Koheron/koheron-server.git
TCP_SERVER_DIR = $(TMP)/$(NAME).koheron-server
TCP_SERVER = $(TCP_SERVER_DIR)/tmp/kserverd
SERVER_CONFIG = $(TMP)/$(NAME).drivers.yml
TCP_SERVER_SHA = master
TCP_SERVER_VENV = $(TMP)/koheron_server_venv
TCP_SERVER_MIDDLEWARE = $(TMP)/$(NAME).middleware

START_SH = $(TMP)/$(NAME).start.sh

ZIP = $(TMP)/$(NAME)-$(VERSION).zip

# App
S3_URL = http://zynq-sdk.s3-website-eu-west-1.amazonaws.com
STATIC_SHA := $(shell curl -s $(S3_URL)/apps | cut -d" " -f1)
STATIC_URL = $(S3_URL)/app-$(STATIC_SHA).zip
STATIC_ZIP = $(TMP)/static.zip

LIVE_DIR = $(TMP)/$(NAME).live

HTTP_API_SRC = $(wildcard os/api/*)
HTTP_API_DIR = $(TMP)/app
HTTP_API_ZIP = app-$(VERSION).zip

METADATA = $(TMP)/metadata.json

.PRECIOUS: $(TMP)/cores/% $(TMP)/%.xpr $(TMP)/%.hwdef $(TMP)/%.bit $(TMP)/%.fsbl/executable.elf $(TMP)/%.tree/system.dts

.PHONY: all linux help debug

all: $(ZIP)

linux: $(ZIP) $(STATIC_ZIP) $(HTTP_API_ZIP) boot.bin uImage devicetree.dtb fw_printenv

help:
	@echo - server: Build the server
	@echo - bd: Build the block design interactively
	@echo - xpr: Build the Vivado project
	@echo - bit: Build the bitstream
	@echo - http: Build the HTTP API
	@echo - run: Run the instrument
	@echo - test: Test the instrument

debug:
	@echo PROJECT DIRECTORY = $(PROJECT_PATH)/$(NAME)
	@echo CORES = $(CORES)
	@echo DRIVERS = $(DRIVERS)
	@echo DRIVERS_LIB = $(DRIVERS_LIB)

$(TMP):
	mkdir -p $(TMP)

###############################################################################
# API
###############################################################################

.PHONY: bd server xpr bit http run

# Run Vivado interactively and build block design
bd: $(CONFIG_TCL) $(XDC) $(PROJECT_PATH)/$(NAME)/*.tcl $(addprefix $(TMP)/cores/, $(CORES))
	vivado -nolog -nojournal -source fpga/scripts/block_design.tcl -tclargs $(NAME) $(PROJECT_PATH) $(PART) $(BOARD) block_design_

server: $(TCP_SERVER)
xpr: $(TMP)/$(NAME).xpr
bit: $(TMP)/$(NAME).bit
http: $(HTTP_API_ZIP)

run: $(ZIP)
	curl -v -F $(NAME)-$(VERSION).zip=@$(ZIP) http://$(HOST)/api/instruments/upload
	curl http://$(HOST)/api/instruments/run/$(NAME)/$(VERSION)
	@echo

###############################################################################
# Tests
###############################################################################

.PHONY: test_module test_core test_driver_% test test_app

test_module: $(CONFIG_TCL) fpga/modules/$(NAME)/*.tcl $(addprefix $(TMP)/cores/, $(CORES))
	vivado -source fpga/scripts/test_module.tcl -tclargs $(NAME) $(PROJECT_PATH) $(PART)

test_core:
	vivado -source fpga/scripts/test_core.tcl -tclargs $(CORE) $(PART)

test_driver_%: drivers/%/test.py
	py.test -v $<

test: $(PROJECT_PATH)/$(NAME)/python/test.py
	py.test -v $<

test_app: os/tests/tests_instrument_manager.py
	py.test -v $<

###############################################################################
# Versioning
###############################################################################

$(VERSION_FILE): | $(TMP)
	echo $(VERSION) > $@
	@echo [$@] OK

$(SHA_FILE):
	echo $(SHA) > $@
	@echo [$@] OK

###############################################################################
# FPGA
###############################################################################

$(CONFIG_TCL): $(MAKE_PY) $(MAIN_YML) $(SHA_FILE) $(TEMPLATE_DIR)/config.tcl
	python $(MAKE_PY) --config_tcl $(NAME) $(PROJECT_PATH)
	@echo [$@] OK

$(TMP)/cores/%: fpga/cores/%/core_config.tcl fpga/cores/%/*.v
	mkdir -p $(@D)
	$(VIVADO) -source fpga/scripts/core.tcl -tclargs $* $(PART)
	@echo [$@] OK

$(TMP)/$(NAME).xpr: $(CONFIG_TCL) $(XDC) $(PROJECT_PATH)/$(NAME)/*.tcl $(addprefix $(TMP)/cores/, $(CORES))
	mkdir -p $(@D)
	$(VIVADO) -source fpga/scripts/project.tcl -tclargs $(NAME) $(PROJECT_PATH) $(PART) $(BOARD)
	@echo [$@] OK

$(TMP)/$(NAME).bit: $(TMP)/$(NAME).xpr
	mkdir -p $(@D)
	$(VIVADO) -source fpga/scripts/bitstream.tcl -tclargs $(NAME)
	@echo [$@] OK

###############################################################################
# First-stage boot loader
###############################################################################

$(TMP)/$(NAME).fsbl/executable.elf: $(TMP)/$(NAME).hwdef
	mkdir -p $(@D)
	$(HSI) -source fpga/scripts/fsbl.tcl -tclargs $(NAME) $(PROC)
	@echo [$@] OK

###############################################################################
# U-Boot
###############################################################################

$(UBOOT_TAR):
	mkdir -p $(@D)
	curl -L $(UBOOT_URL) -o $@
	@echo [$@] OK

$(UBOOT_DIR): $(UBOOT_TAR)
	mkdir -p $@
	tar -zxf $< --strip-components=1 --directory=$@
	patch -d $(TMP) -p 0 < $(PATCHES)/u-boot-xlnx-$(UBOOT_TAG).patch
	bash $(PATCHES)/uboot.sh $(PATCHES) $@
	@echo [$@] OK

$(TMP)/u-boot.elf: $(UBOOT_DIR)
	mkdir -p $(@D)
	make -C $< mrproper
	make -C $< arch=arm `find $(PATCHES) -name '*_defconfig' -exec basename {} \;`
	make -C $< arch=arm CFLAGS=$(UBOOT_CFLAGS) \
	  CROSS_COMPILE=arm-linux-gnueabihf- all
	cp $</u-boot $@
	@echo [$@] OK

fw_printenv: $(UBOOT_DIR) $(TMP)/u-boot.elf
	make -C $< arch=ARM CFLAGS=$(ARMHF_CFLAGS) \
	  CROSS_COMPILE=arm-linux-gnueabihf- env
	cp $</tools/env/fw_printenv $@
	@echo [$@] OK

###############################################################################
# boot.bin
###############################################################################

boot.bin: $(TMP)/$(NAME).fsbl/executable.elf $(TMP)/$(NAME).bit $(TMP)/u-boot.elf
	echo "img:{[bootloader] $^}" > $(TMP)/boot.bif
	bootgen -image $(TMP)/boot.bif -w -o i $@
	@echo [$@] OK

###############################################################################
# Device tree
###############################################################################

$(TMP)/$(NAME).hwdef: $(TMP)/$(NAME).xpr
	mkdir -p $(@D)
	$(VIVADO) -source fpga/scripts/hwdef.tcl -tclargs $(NAME)
	@echo [$@] OK

$(DTREE_TAR):
	mkdir -p $(@D)
	curl -L $(DTREE_URL) -o $@
	@echo [$@] OK

$(DTREE_DIR): $(DTREE_TAR)
	mkdir -p $@
	tar -zxf $< --strip-components=1 --directory=$@
	@echo [$@] OK

$(TMP)/$(NAME).tree/system.dts: $(TMP)/$(NAME).hwdef $(DTREE_DIR)
	mkdir -p $(@D)
	$(HSI) -source fpga/scripts/devicetree.tcl -tclargs $(NAME) $(PROC) $(DTREE_DIR) $(VIVADO_VERSION)
	patch $@ $(PATCHES)/devicetree.patch
	@echo [$@] OK

###############################################################################
# Linux
###############################################################################

$(RTL_TAR):
	mkdir -p $(@D)
	curl -L $(RTL_URL) -o $@
	@echo [$@] OK

$(LINUX_TAR):
	mkdir -p $(@D)
	curl -L $(LINUX_URL) -o $@
	@echo [$@] OK

$(LINUX_DIR): $(LINUX_TAR) $(RTL_TAR)
	mkdir -p $@
	tar -zxf $< --strip-components=1 --directory=$@
	tar -zxf $(RTL_TAR) --directory=$@/drivers/net/wireless/realtek
	patch -d $(TMP) -p 0 < $(PATCHES)/linux-xlnx-$(LINUX_TAG).patch
	bash $(PATCHES)/linux.sh $(PATCHES) $@
	@echo [$@] OK

uImage: $(LINUX_DIR)
	make -C $< mrproper
	make -C $< ARCH=arm xilinx_zynq_defconfig
	make -C $< ARCH=arm CFLAGS=$(LINUX_CFLAGS) \
	  -j $(shell nproc 2> /dev/null || echo 1) \
	  CROSS_COMPILE=arm-linux-gnueabihf- UIMAGE_LOADADDR=0x8000 uImage
	cp $</arch/arm/boot/uImage $@
	@echo [$@] OK

devicetree.dtb: uImage $(TMP)/$(NAME).tree/system.dts
	$(LINUX_DIR)/scripts/dtc/dtc -I dts -O dtb -o devicetree.dtb \
	  -i $(TMP)/$(NAME).tree $(TMP)/$(NAME).tree/system.dts
	@echo [$@] OK

###############################################################################
# koheron-server (compiled with project specific middleware)
###############################################################################

$(TCP_SERVER_DIR):
	test -d $(TCP_SERVER_DIR) || git clone $(TCP_SERVER_URL) $(TCP_SERVER_DIR)
	cd $(TCP_SERVER_DIR) && git checkout $(TCP_SERVER_SHA)
	echo `cd $(TCP_SERVER_DIR) && git rev-parse HEAD` > $(TCP_SERVER_DIR)/VERSION
	@echo [$@] OK

$(TCP_SERVER_DIR)/requirements.txt: $(TCP_SERVER_DIR)

$(TCP_SERVER_VENV): $(TCP_SERVER_DIR)/requirements.txt
ifeq ($(DOCKER),True)
	/usr/bin/pip install -r $(TCP_SERVER_DIR)/requirements.txt
else
	test -d $(TCP_SERVER_VENV) || virtualenv $(TCP_SERVER_VENV)
	$(TCP_SERVER_VENV)/bin/pip install -r $(TCP_SERVER_DIR)/requirements.txt
endif

$(TCP_SERVER_MIDDLEWARE)/%: %
	mkdir -p -- `dirname -- $@`
	cp $^ $@
	@echo [$@] OK

$(TCP_SERVER): $(MAKE_PY) $(TCP_SERVER_VENV) $(SERVER_CONFIG) \
               $(addprefix $(TCP_SERVER_MIDDLEWARE)/, $(DRIVERS)) \
               $(DRIVERS_LIB) projects/default/server.yml
	python $(MAKE_PY) --middleware $(NAME) $(PROJECT_PATH)
	cp -R drivers/lib $(TCP_SERVER_MIDDLEWARE)/drivers/
	cd $(TCP_SERVER_DIR) && make CONFIG=$(SERVER_CONFIG) BASE_DIR=../.. \
	  PYTHON=$(PYTHON) MIDWARE_PATH=$(TCP_SERVER_MIDDLEWARE) clean all
	@echo [$@] OK

###############################################################################
# Instrument ZIP file (contains bitstream and server)
###############################################################################

$(START_SH): $(MAKE_PY) $(MAIN_YML) $(TEMPLATE_DIR)/start.sh
	python $(MAKE_PY) --start_sh $(NAME) $(PROJECT_PATH)
	@echo [$@] OK

$(LIVE_DIR): $(TMP) $(MAIN_YML)
	python $(MAKE_PY) --live_zip $(NAME) $(PROJECT_PATH)
	@echo [$@] OK

$(ZIP): $(TCP_SERVER) $(VERSION_FILE) $(PYTHON_DIR) $(TMP)/$(NAME).bit $(START_SH) $(LIVE_DIR)
	zip --junk-paths $(ZIP) $(TMP)/$(NAME).bit $(TMP)/$(NAME).live $(TCP_SERVER) $(START_SH) $(LIVE_DIR)/*
	@echo [$@] OK

###############################################################################
# HTTP API
###############################################################################

.PHONY: app_sync app_sync_ssh

$(METADATA): $(MAKE_PY) $(VERSION_FILE)
	python $(MAKE_PY) --metadata $(NAME) $(VERSION)
	@echo [$@] OK

$(HTTP_API_DIR): $(HTTP_API_SRC) $(METADATA)
	mkdir -p $(HTTP_API_DIR)/api_app
	cp -R os/api/. $(HTTP_API_DIR)/api_app
	cp $(TMP)/metadata.json $(HTTP_API_DIR)
	cp os/wsgi.py $(HTTP_API_DIR)
	@echo [$@] OK

$(HTTP_API_ZIP): $(HTTP_API_DIR)
	cd $(HTTP_API_DIR) && zip -r $(HTTP_API_ZIP) .
	@echo [$@] OK

app_sync: $(HTTP_API_ZIP)
	curl -v -F app-$(VERSION).zip=@$(HTTP_API_DIR)/$(HTTP_API_ZIP) http://$(HOST)/api/app/update
	@echo

# To use if uwsgi is not running
app_sync_ssh: $(HTTP_API_ZIP)
	rsync -avz -e "ssh -i /ssh-private-key" $(HTTP_API_DIR)/. root@$(HOST):/usr/local/flask/

###############################################################################
# Static content served by NGINX
###############################################################################

$(STATIC_ZIP): $(TMP)
	echo $(STATIC_SHA)
	curl -L $(STATIC_URL) -o $(STATIC_ZIP)

###############################################################################
# Clean target
###############################################################################

.PHONY: clean clean_project clean_server clean_cores clean_xpr

clean:
	$(RM) uImage fw_printenv boot.bin devicetree.dtb $(TMP)
	$(RM) .Xil usage_statistics_webtalk.html usage_statistics_webtalk.xml
	$(RM) webtalk*.log webtalk*.jou

clean_project:
	$(RM) $(TMP)/$(NAME).* $(TMP)/$(NAME)-*.zip

clean_server:
	$(RM) $(TCP_SERVER_DIR) $(TCP_SERVER_MIDDLEWARE)

clean_cores:
	$(RM) $(TMP)/cores

clean_core:
	$(RM) $(TMP)/cores/$(CORE)*

clean_xpr:
	$(RM) $(TMP)/$(NAME).xpr
