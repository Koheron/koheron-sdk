ARG UBUNTU_VERSION=26.04
FROM ubuntu:${UBUNTU_VERSION}
ARG GCC_VERSION=15
ENV DEBIAN_FRONTEND=noninteractive

# Native management tools use the same packaged libraries on both board ABIs.
# Keep native repositories restricted to amd64; cross libraries come from ports.
RUN dpkg --add-architecture armhf && dpkg --add-architecture arm64 \
    && sed -i '/^Types: deb/a Architectures: amd64' /etc/apt/sources.list.d/ubuntu.sources \
    && . /etc/os-release \
    && printf 'deb [arch=armhf,arm64] http://ports.ubuntu.com/ubuntu-ports %s main universe\ndeb [arch=armhf,arm64] http://ports.ubuntu.com/ubuntu-ports %s-updates main universe\ndeb [arch=armhf,arm64] http://ports.ubuntu.com/ubuntu-ports %s-security main universe\n' \
       "$VERSION_CODENAME" "$VERSION_CODENAME" "$VERSION_CODENAME" > /etc/apt/sources.list.d/koheron-cross.list

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ca-certificates wget unzip xz-utils curl \
    binutils-aarch64-linux-gnu binutils-arm-linux-gnueabihf \
    gcc-${GCC_VERSION} g++-${GCC_VERSION} \
    gcc-${GCC_VERSION}-arm-linux-gnueabihf g++-${GCC_VERSION}-arm-linux-gnueabihf \
    gcc-${GCC_VERSION}-aarch64-linux-gnu g++-${GCC_VERSION}-aarch64-linux-gnu \
    gcc-arm-none-eabi \
    flex bison bc qemu-user zerofree u-boot-tools libgnutls28-dev \
    libeigen3-dev \
    libnewlib-arm-none-eabi \
    libssl-dev zlib1g-dev uuid-dev pkg-config \
    libmicrohttpd-dev libzip-dev libjson-c-dev libsystemd-dev libunistring-dev \
    libmicrohttpd-dev:armhf libzip-dev:armhf libjson-c-dev:armhf libsystemd-dev:armhf libunistring-dev:armhf \
    libmicrohttpd-dev:arm64 libzip-dev:arm64 libjson-c-dev:arm64 libsystemd-dev:arm64 libunistring-dev:arm64 \
    swig python3 python3-dev python3-setuptools device-tree-compiler \
    fdisk util-linux parted udev dosfstools e2fsprogs zip eatmydata \
    ccache gettext-base \
    && if [ "$(. /etc/os-release; echo "$VERSION_ID")" = 24.04 ]; then apt-get install -y --no-install-recommends qemu-user-static; fi \
    && rm -rf /var/lib/apt/lists/*

# Preserve the rootfs scripts' QEMU paths; newer Ubuntu names static binaries
# qemu-arm/qemu-aarch64, while 24.04 provides separate *-static binaries.
RUN for arch in arm aarch64; do \
      if [ ! -x /usr/bin/qemu-${arch}-static ]; then \
        ln -s /usr/bin/qemu-${arch} /usr/bin/qemu-${arch}-static; \
      fi; \
      ! readelf -l /usr/bin/qemu-${arch}-static | grep -q INTERP || exit 1; \
    done

# Eigen compatibility headers
RUN ln -s /usr/include/eigen3/Eigen /usr/include/Eigen && \
    ln -s /usr/include/eigen3/unsupported /usr/include/unsupported

# ---- aarch64-none-elf (bare-metal) toolchain from Arm ----
ARG AARCH64_NONE_ELF_VER=14.3.rel1
ARG AARCH64_NONE_ELF_BASE=arm-gnu-toolchain-${AARCH64_NONE_ELF_VER}-x86_64-aarch64-none-elf
ARG AARCH64_NONE_ELF_URL=https://developer.arm.com/-/media/Files/downloads/gnu/${AARCH64_NONE_ELF_VER}/binrel/${AARCH64_NONE_ELF_BASE}.tar.xz
ARG AARCH64_NONE_ELF_SHA256=ebaf2d47f2e7f7b645864c5c8cf839e526daed83a2e675a3525d03f5ba3d2be9

RUN curl -fsSL "${AARCH64_NONE_ELF_URL}" -o /tmp/agt.tar.xz \
 && echo "${AARCH64_NONE_ELF_SHA256}  /tmp/agt.tar.xz" | sha256sum -c - \
 && mkdir -p /opt \
 && tar -xJf /tmp/agt.tar.xz -C /opt \
 && ln -s /opt/${AARCH64_NONE_ELF_BASE} /opt/aarch64-none-elf \
 && rm -f /tmp/agt.tar.xz

# Put it on PATH for all subsequent stages
ENV PATH="/opt/aarch64-none-elf/bin:${PATH}"

# Check the packaged native and cross compiler versions.
RUN aarch64-none-elf-gcc --version \
    && test "$(gcc-${GCC_VERSION} -dumpversion)" = "$GCC_VERSION" \
    && test "$(arm-linux-gnueabihf-gcc-${GCC_VERSION} -dumpversion)" = "$GCC_VERSION" \
    && test "$(aarch64-linux-gnu-gcc-${GCC_VERSION} -dumpversion)" = "$GCC_VERSION"

ENV TERM=xterm-256color
