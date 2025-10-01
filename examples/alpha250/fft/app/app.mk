
APP_PATH := $(PROJECT_PATH)/app

# -----------------------------------------------------------------------------
# Build directory
# -----------------------------------------------------------------------------

TMP_SERVER_PATH := $(TMP_PROJECT_PATH)/server

# ensure tmp dir exists early
$(shell mkdir -p '$(TMP_SERVER_PATH)' >/dev/null 2>&1)
$(TMP_SERVER_PATH)/: ; @mkdir -p $@

# -----------------------------------------------------------------------------
# Compiler
# -----------------------------------------------------------------------------

SERVER_CCXX = $(DOCKER) $(GCC_ARCH)-g++-$(GCC_VERSION) -flto=$(N_CPUS)

SERVER_CCXXFLAGS = -Wall -Werror -Wextra
SERVER_CCXXFLAGS += -Wpedantic -Wfloat-equal -Wunused-macros -Wcast-qual -Wuseless-cast
SERVER_CCXXFLAGS += -Wlogical-op -Wdouble-promotion -Wformat -Wmissing-include-dirs -Wundef
SERVER_CCXXFLAGS += -Wpacked -Wredundant-decls -Wvarargs -Wvector-operation-performance -Wswitch-default
SERVER_CCXXFLAGS += -Wuninitialized  -Wmissing-declarations
SERVER_CCXXFLAGS += -Wno-psabi
SERVER_CCXXFLAGS += -DINSTRUMENT_NAME=$(NAME)
SERVER_CCXXFLAGS += -I$(APP_PATH) -I$(TMP_SERVER_PATH) -I$(TMP_SERVER_PATH) -I$(SERVER_PATH)/external_libs -I$(SERVER_PATH)/runtime -I$(SDK_PATH) -I. -I$(SERVER_PATH)/context -I$(SERVER_PATH)/drivers -I$(PROJECT_PATH)
SERVER_CCXXFLAGS += -O3 -fno-math-errno
SERVER_CCXXFLAGS += -MMD -MP -static-libstdc++ $(GCC_FLAGS)
SERVER_CCXXFLAGS += -std=c++20 -pthread

# -----------------------------------------------------------------------------
# Memory header from YAML
# -----------------------------------------------------------------------------

$(TMP_SERVER_PATH)/memory.hpp: $(MEMORY_YML) $(SERVER_PATH)/templates/memory.hpp
	$(MAKE_PY) --memory_hpp $@ $(MEMORY_YML)

# -----------------------------------------------------------------------------
# Generate drivers include
# -----------------------------------------------------------------------------

# Target to generate drivers.hpp
$(TMP_SERVER_PATH)/drivers.hpp: $(DRIVERS)
	@echo "Generating $@"
	@{ \
	  echo "// Auto-generated file. Do not edit."; \
	  echo "#ifndef __GENERATED_DRIVERS_HPP__"; \
	  echo "#define __GENERATED_DRIVERS_HPP__"; \
	  echo; \
	  for d in $(DRIVERS); do \
	    echo "#include \"$$d\""; \
	  done; \
	  echo; \
	  echo "#endif // __GENERATED_DRIVERS_HPP__"; \
	} > $@

# -----------------------------------------------------------------------------
# Object files
# -----------------------------------------------------------------------------

DRIVERS_CPP := $(filter %.cpp,$(DRIVERS))
DRIVERS_OBJ := $(addprefix $(TMP_SERVER_PATH)/, $(subst .cpp,.o,$(notdir $(filter %.cpp,$(DRIVERS)))))
SERVER_LIB_OBJ := $(subst .cpp,.o, $(addprefix $(TMP_SERVER_PATH)/, $(notdir $(wildcard $(SERVER_PATH)/runtime/*.cpp))))
CONTEXT_OBJS := $(TMP_SERVER_PATH)/spi_dev.o $(TMP_SERVER_PATH)/i2c_dev.o

APP_OBJ := $(TMP_SERVER_PATH)/main.o
OBJ := $(APP_OBJ) $(SERVER_LIB_OBJ) $(DRIVERS_OBJ) $(CONTEXT_OBJS)

# -----------------------------------------------------------------------------
# Compile / Link
# -----------------------------------------------------------------------------

GEN_HDRS := \
  $(TMP_SERVER_PATH)/memory.hpp \
  $(TMP_SERVER_PATH)/drivers.hpp

$(TMP_SERVER_PATH)/%.o: $(APP_PATH)/%.cpp $(GEN_HDRS)
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/runtime/%.cpp $(GEN_HDRS)
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(TMP_SERVER_PATH)/%.o: $(SERVER_PATH)/context/%.cpp $(GEN_HDRS)
	$(SERVER_CCXX) -c $(SERVER_CCXXFLAGS) -o $@ $<

$(SERVER): $(OBJ) | $(KOHERON_SERVER_PATH)
	$(SERVER_CCXX) -o $@ $(OBJ) $(SERVER_CCXXFLAGS) -lm
	$(call ok,$@)

.PHONY: server
server: $(SERVER)
	$(MAKE) --jobs=$(N_CPUS) $(SERVER)

.PHONY: clean_server
clean_server:
	rm -rf $(TMP_SERVER_PATH)
