# Both analyzers run on Cortex-A9 CPUs with NEON.
DRIVERS += $(SDK_PATH)/server/external_libs/pffft/pffft.cpp
SERVER_EXTRA_CCXXFLAGS += -mfpu=neon
WEB_FILES += $(SDK_PATH)/server/external_libs/pffft/pffft-LICENSE.txt
