APP_PATH := $(PROJECT_PATH)
include $(dir $(lastword $(MAKEFILE_LIST)))build-common.mk

# Runtime / Hardware objects
OBJ = $(TMP_SERVER_PATH)/systemd.o \
      $(TMP_SERVER_PATH)/signal_handler.o \
      $(TMP_SERVER_PATH)/runtime_executor.o

SERVER_CPP := $(wildcard $(SERVER_PATH)/network/*.cpp) \
              $(wildcard $(SERVER_PATH)/utilities/*.cpp)
SERVER_OBJ := $(subst .cpp,.o, $(addprefix $(TMP_SERVER_PATH)/, $(notdir $(SERVER_CPP))))

HARDWARE_OBJ := $(TMP_SERVER_PATH)/fpga_manager.o $(TMP_SERVER_PATH)/zynq_fclk.o

SERVERLESS_OBJ := $(subst .cpp,.o, $(addprefix $(TMP_SERVER_PATH)/, $(notdir $(SERVERLESS_CPP_SRCS))))

OBJ += $(SERVERLESS_OBJ) $(SERVER_OBJ) $(HARDWARE_OBJ)

GEN_HEADERS := $(TMP_SERVER_PATH)/memory.hpp
include $(SERVER_BUILD_PATH)build-rules.mk

.PHONY: server
server: $(SERVER)
