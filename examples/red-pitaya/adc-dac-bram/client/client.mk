TMP_CLIENT_PATH := $(TMP_PROJECT_PATH)/client

$(TMP_CLIENT_PATH):
	@mkdir -p $@

CLIENT := $(TMP_CLIENT_PATH)/main

OPERATIONS_HPP = $(TMP_SERVER_PATH)/operations.hpp

# Doesn't build with GCC
# cf. https://stackoverflow.com/questions/41932777/incomplete-type-on-gcc-x86-but-not-on-arm?noredirect=1&lq=1

#CLIENT_CCXX := g++-7 -flto
#CLIENT_CCXX = x86_64-w64-mingw32-g++

# Install clang:
# apt-get install clang libc++-dev
CLIENT_CCXX := clang

# Common flags
CLIENT_CCXXFLAGS := -march=native -O3
CLIENT_CCXXFLAGS += -MMD -MP -Wall -Werror
CLIENT_CCXXFLAGS += -std=c++14 -pthread
CLIENT_CCXXFLAGS += -I$(SDK_PATH) -I. -I$(SERVER_PATH)/context -I$(SERVER_PATH)/core -I$(TMP_SERVER_PATH) -I$(SERVER_PATH)/client

# GCC flags
# CLIENT_CCXXFLAGS += -lm -static-libgcc -static-libstdc++
# CLIENT_CCXXFLAGS += -Wpedantic -Wfloat-equal -Wunused-macros -Wcast-qual -Wuseless-cast
# CLIENT_CCXXFLAGS += -Wlogical-op -Wdouble-promotion -Wformat -Wmissing-include-dirs -Wundef
# CLIENT_CCXXFLAGS += -Wcast-align -Wpacked -Wredundant-decls -Wvarargs -Wvector-operation-performance -Wswitch-default
# CLIENT_CCXXFLAGS += -Wuninitialized -Wshadow -Wzero-as-null-pointer-constant -Wmissing-declarations

# Clang flags
CLIENT_CCXXFLAGS += -stdlib=libc++
CLIENT_LD_FLAGS=-fuse-ld=gold -lstdc++

# Mingw32 flags
#CLIENT_CCXXFLAGS += -lws2_32

CLIENT_OBJ := $(TMP_CLIENT_PATH)/main.o
CLIENT_DEP=$(subst .o,.d,$(OBJ))
-include $(CLIENT_DEP)

$(TMP_CLIENT_PATH)/%.o: $(CLIENT_PATH)/%.cpp $(OPERATIONS_HPP) | $(TMP_CLIENT_PATH)
	$(CLIENT_CCXX) -c $(CLIENT_CCXXFLAGS) -o $@ $<

PHONY: client
client: server | $(CLIENT_OBJ)
	$(CLIENT_CCXX) -o $(CLIENT) $(CLIENT_OBJ) $(CLIENT_CCXXFLAGS) $(CLIENT_LD_FLAGS)

PHONY: run_client
run_client: client
	$(CLIENT) $(HOST)

PHONY: clean_client
clean_client:
	rm -rf $(TMP_CLIENT_PATH)