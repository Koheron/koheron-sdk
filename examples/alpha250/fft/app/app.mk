# Custom entry point and executor; share the SDK compiler and cache tracking.
APP_PATH := $(PROJECT_PATH)/app
SERVER_WARNING_FLAGS := -Werror
include $(SERVER_PATH)/build-common.mk

# Use the configured drivers for both includes and the runtime type table.
DRIVERS_HPP := $(filter %.hpp,$(DRIVERS))
export DRIVERS_HPP
DRIVERS_LIST_FILE := $(TMP_SERVER_PATH)/drivers.list
$(eval $(call build_settings_stamp,$(DRIVERS_LIST_FILE),$$(foreach H,$$(DRIVERS_HPP),$$(call build_shell_quote,$$(patsubst ./%,%,$$(H))))))

GEN_HEADERS := $(TMP_SERVER_PATH)/memory.hpp $(TMP_SERVER_PATH)/drivers.hpp $(TMP_SERVER_PATH)/drivers_list.hpp
$(TMP_SERVER_PATH)/drivers.hpp $(TMP_SERVER_PATH)/drivers_list.hpp: $(TMP_SERVER_PATH)/%: $(SERVER_PATH)/templates/% $(DRIVERS_HPP) $(DRIVERS_LIST_FILE) $(MEMORY_YML) $(MAKE_PY_SOURCES) | $(TMP_SERVER_PATH)/ $(PYTHON_REQUIREMENTS_STAMP)
	$(call echo-cmd,tpl)
	$(Q)$(MAKE_PY) --render_template $@ $(MEMORY_YML) $<

DRIVERS_CPP := $(filter %.cpp,$(DRIVERS))
DRIVERS_CPP_REL := $(patsubst $(SDK_FULL_PATH)/%,%,$(DRIVERS_CPP))
DRIVERS_OBJ := $(addprefix $(TMP_SERVER_PATH)/,$(DRIVERS_CPP_REL:.cpp=.o))
SERVER_LIB_OBJ := $(subst .cpp,.o, $(addprefix $(TMP_SERVER_PATH)/, $(notdir $(wildcard $(SERVER_PATH)/runtime/*.cpp))))
SERVER_NET_OBJ := $(subst .cpp,.o, $(addprefix $(TMP_SERVER_PATH)/, $(notdir $(wildcard $(SERVER_PATH)/network/*.cpp))))
SERVER_UTIL_OBJ := $(subst .cpp,.o, $(addprefix $(TMP_SERVER_PATH)/, $(notdir $(wildcard $(SERVER_PATH)/utilities/*.cpp))))
HARDWARE_OBJ := $(TMP_SERVER_PATH)/spi_manager.o $(TMP_SERVER_PATH)/i2c_manager.o $(TMP_SERVER_PATH)/fpga_manager.o $(TMP_SERVER_PATH)/zynq_fclk.o

# Add here other object files needed by the app
APP_OBJ := $(TMP_SERVER_PATH)/main.o

OBJ := $(APP_OBJ) $(SERVER_LIB_OBJ) $(DRIVERS_OBJ) $(HARDWARE_OBJ) $(SERVER_NET_OBJ) $(SERVER_UTIL_OBJ)
include $(SERVER_BUILD_PATH)build-rules.mk

.PHONY: server
server: $(SERVER)
