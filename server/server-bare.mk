
APP_PATH := $(PROJECT_PATH)

# -----------------------------------------------------------------------------
# Build directory
# -----------------------------------------------------------------------------

TMP_SERVER_PATH := $(TMP_PROJECT_PATH)/server/

# -----------------------------------------------------------------------------
# Compiler
# -----------------------------------------------------------------------------

SERVER_CCXX = $(DOCKER) $(GCC_ARCH)-g++-$(GCC_VERSION) -flto=$(N_CPUS)

SERVER_CCXXFLAGS = -Wall -Wextra
#SERVER_CCXXFLAGS += -Werror
SERVER_CCXXFLAGS += -Wpedantic -Wfloat-equal -Wunused-macros -Wcast-qual -Wuseless-cast
SERVER_CCXXFLAGS += -Wlogical-op -Wdouble-promotion -Wformat -Wmissing-include-dirs -Wundef
SERVER_CCXXFLAGS += -Wpacked -Wredundant-decls -Wvarargs -Wvector-operation-performance -Wswitch-default
SERVER_CCXXFLAGS += -Wuninitialized  -Wmissing-declarations
SERVER_CCXXFLAGS += -Wno-psabi
SERVER_CCXXFLAGS += -DINSTRUMENT_NAME=\"$(NAME)\"
SERVER_CCXXFLAGS += -I$(APP_PATH) -I$(TMP_SERVER_PATH) -I$(TMP_SERVER_PATH) \
    -I$(SERVER_PATH)/external_libs -I$(SERVER_PATH)/runtime -I$(SERVER_PATH)/hardware \
	-I$(SDK_PATH) -I. -I$(SERVER_PATH)/context -I$(SERVER_PATH)/drivers -I$(PROJECT_PATH)
SERVER_CCXXFLAGS += -O3 -fno-math-errno
SERVER_CCXXFLAGS += -MMD -MP -static-libstdc++ $(GCC_FLAGS)
SERVER_CCXXFLAGS += -std=c++20 -pthread

# -----------------------------------------------------------------------------
# Memory header from YAML
# -----------------------------------------------------------------------------

$(TMP_SERVER_PATH)/memory.hpp: $(MEMORY_YML) $(SERVER_PATH)/templates/memory.hpp | $(TMP_SERVER_PATH)
	$(MAKE_PY) --memory_hpp $@ $(MEMORY_YML)
	$(call ok,$@)

# -----------------------------------------------------------------------------
# Object files
# -----------------------------------------------------------------------------

# Runtime / Hardware objects
OBJ = 	$(TMP_SERVER_PATH)/systemd.o \
	 	$(TMP_SERVER_PATH)/signal_handler.o \
		$(TMP_SERVER_PATH)/runtime_executor.o \

SERVER_CPP := $(wildcard $(SERVER_PATH)/network/*.cpp) \
              $(wildcard $(SERVER_PATH)/utilities/*.cpp)
SERVER_OBJ := $(subst .cpp,.o, $(addprefix $(TMP_SERVER_PATH)/, $(notdir $(SERVER_CPP))))

HARDWARE_OBJ := $(TMP_SERVER_PATH)/fpga_manager.o $(TMP_SERVER_PATH)/zynq_fclk.o

SERVERLESS_OBJ := $(subst .cpp,.o, $(addprefix $(TMP_SERVER_PATH)/, $(notdir $(SERVERLESS_CPP_SRCS))))

OBJ += $(SERVERLESS_OBJ) $(SERVER_OBJ) $(HARDWARE_OBJ)
DEP := $(subst .o,.d,$(OBJ))
-include $(DEP)

# -----------------------------------------------------------------------------
# Compile / Link
# -----------------------------------------------------------------------------

GEN_HDRS := \
  $(TMP_SERVER_PATH)/memory.hpp

$(TMP_SERVER_PATH)/%.o: $(APP_PATH)/%.cpp $(TMP_SERVER_PATH)/memory.hpp | $(TMP_SERVER_PATH)
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/runtime/%.cpp | $(TMP_SERVER_PATH)
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/hardware/%.cpp | $(TMP_SERVER_PATH)
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/network/%.cpp | $(TMP_SERVER_PATH)
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/utilities/%.cpp | $(GEN_HEADERS)
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(SERVER): $(OBJ) $(GEN_HDRS) | $(KOHERON_SERVER_PATH)
	$(SERVER_CCXX) -o $@ $(OBJ) $(SERVER_CCXXFLAGS) -lm
	$(call ok,$@)

.PHONY: server
server: $(SERVER)
	$(MAKE) --jobs=$(N_CPUS) $(SERVER)

.PHONY: clean_server
clean_server:
	rm -rf $(TMP_SERVER_PATH)
