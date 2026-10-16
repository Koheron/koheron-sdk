include $(SDK_PATH)/server/server.mk

SPI_VENDOR_OBJ := $(addprefix $(TMP_SERVER_PATH)/xilinx/,xspi.o xspi_options.o xspi_selftest.o)
OBJ += $(SPI_VENDOR_OBJ)

# The builder contains pinned sources. Compile them as C, with the local port.
# Track the Dockerfile and port headers explicitly: /opt source paths exist only
# inside the image and cannot be included in host Make dependency files.
$(SPI_VENDOR_OBJ): $(PROJECT_PATH)/Dockerfile $(PROJECT_PATH)/server.mk \
    $(wildcard $(PROJECT_PATH)/port/*.h) $(SERVER_COMPILER_STAMP) $(SERVER_FLAGS_STAMP)
$(TMP_SERVER_PATH)/xilinx/%.o:
	@mkdir -p $(@D)
	$(DOCKER) $(GCC_ARCH)-gcc-$(GCC_VERSION) -std=c11 -O2 -Wall -Wextra -Werror $(GCC_FLAGS) \
		-isystem /opt/libmetal/include -isystem /opt/xilinx-spi -I$(PROJECT_PATH)/port \
		-c /opt/xilinx-spi/$*.c -o $@

# Static third-party dependencies: no extra libraries need installing on the board.
cmd_link += /opt/libmetal/lib/libmetal.a /usr/lib/arm-linux-gnueabihf/libsysfs.a
$(SERVER): $(SPI_VENDOR_OBJ)
