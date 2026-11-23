ARG BASE_IMAGE=cross-armhf:26.04
FROM ${BASE_IMAGE}

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake libsysfs-dev libsysfs-dev:armhf \
    && rm -rf /var/lib/apt/lists/*

# libmetal v2025.10.0 (1.9.0), pinned by commit and archive digest.
RUN curl -fsSL https://codeload.github.com/OpenAMP/libmetal/tar.gz/dbf9cf5286b4d7d44b930344398045916ebe8b8b -o /tmp/libmetal.tar.gz \
    && echo '557b0ca7558a1c5a5333cac205de459912589f01db7367755bcdf694e02061f0  /tmp/libmetal.tar.gz' | sha256sum -c - \
    && mkdir /tmp/libmetal \
    && tar xzf /tmp/libmetal.tar.gz --strip-components=1 -C /tmp/libmetal \
    && cmake -S /tmp/libmetal -B /tmp/metal-arm \
       -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=arm \
       -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc-15 \
       -DLIBSYSFS_LIBRARY=/usr/lib/arm-linux-gnueabihf/libsysfs.a \
       -DCMAKE_INSTALL_PREFIX=/opt/libmetal \
       -DWITH_SHARED_LIB=OFF -DWITH_TESTS=OFF -DWITH_DOC=OFF \
    && cmake --build /tmp/metal-arm -j4 \
    && cmake --install /tmp/metal-arm \
    && cmake -S /tmp/libmetal -B /tmp/metal-host \
       -DCMAKE_C_COMPILER=gcc-15 -DCMAKE_INSTALL_PREFIX=/opt/libmetal-native \
       -DWITH_SHARED_LIB=OFF -DWITH_TESTS=OFF -DWITH_DOC=OFF \
    && cmake --build /tmp/metal-host -j4 \
    && cmake --install /tmp/metal-host \
    && cp /tmp/libmetal/LICENSE.md /opt/libmetal/LICENSE.md \
    && rm -rf /tmp/libmetal /tmp/libmetal.tar.gz /tmp/metal-arm /tmp/metal-host

# Unmodified XSpi sources and portable BSP headers from xilinx_v2025.1.
# The example supplies Linux replacements for xil_io.h and xil_util.h.
RUN set -eu; \
    base=https://raw.githubusercontent.com/Xilinx/embeddedsw/c0aed2eff7a30f307238ec853fa8fbc45dcabdda; \
    mkdir /opt/xilinx-spi; \
    for file in xspi.c xspi_options.c xspi_selftest.c xspi.h xspi_i.h xspi_l.h; do \
        curl -fsSL "$base/XilinxProcessorIPLib/drivers/spi/src/$file" -o "/opt/xilinx-spi/$file"; \
    done; \
    for file in xil_types.h xil_assert.h xstatus.h; do \
        curl -fsSL "$base/lib/bsp/standalone/src/common/$file" -o "/opt/xilinx-spi/$file"; \
    done; \
    curl -fsSL "$base/LICENSES/MIT" -o /opt/xilinx-spi/LICENSE
