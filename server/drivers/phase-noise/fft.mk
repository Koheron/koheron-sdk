# All three analyzers run on Cortex-A9 CPUs with NEON. PFFFT's vendor
# implementation wrapper enables its NEON backend explicitly.
DRIVERS += $(SDK_PATH)/server/external_libs/pffft/pffft.cpp
SERVER_EXTRA_CCXXFLAGS += -mfpu=neon
PNA_PUBLICATION_HZ ?= 30
SERVER_EXTRA_CCXXFLAGS += -DKOHERON_PNA_PUBLICATION_HZ=$(PNA_PUBLICATION_HZ)
WEB_FILES += $(SDK_PATH)/server/external_libs/pffft/pffft-LICENSE.txt
