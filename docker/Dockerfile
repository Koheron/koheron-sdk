FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ca-certificates wget unzip xz-utils curl \
    binutils-aarch64-linux-gnu binutils-arm-linux-gnueabihf \
    gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf \
    gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
    gcc-arm-none-eabi \
    flex bison bc qemu-user-static zerofree u-boot-tools libgnutls28-dev \
    libeigen3-dev \
    libnewlib-arm-none-eabi \
    libssl-dev zlib1g-dev pkg-config \
    swig python3 python3-dev python3-setuptools device-tree-compiler \
    fdisk util-linux parted udev dosfstools e2fsprogs zip eatmydata \
    ccache \
    && rm -rf /var/lib/apt/lists/*

# Optional Eigen shim (see above)
RUN ln -s /usr/include/eigen3/Eigen /usr/include/Eigen && \
    ln -s /usr/include/eigen3/unsupported /usr/include/unsupported

# ---- aarch64-none-elf (bare-metal) toolchain from Arm ----
ARG AARCH64_NONE_ELF_VER=14.3.rel1
ARG AARCH64_NONE_ELF_BASE=arm-gnu-toolchain-${AARCH64_NONE_ELF_VER}-x86_64-aarch64-none-elf
ARG AARCH64_NONE_ELF_URL=https://developer.arm.com/-/media/Files/downloads/gnu/${AARCH64_NONE_ELF_VER}/binrel/${AARCH64_NONE_ELF_BASE}.tar.xz

RUN curl -fsSL "${AARCH64_NONE_ELF_URL}" -o /tmp/agt.tar.xz \
 && mkdir -p /opt \
 && tar -xJf /tmp/agt.tar.xz -C /opt \
 && ln -s /opt/${AARCH64_NONE_ELF_BASE} /opt/aarch64-none-elf \
 && rm -f /tmp/agt.tar.xz

# Put it on PATH for all subsequent stages
ENV PATH="/opt/aarch64-none-elf/bin:${PATH}"

# Quick sanity check (optional)
RUN aarch64-none-elf-gcc --version

ENV TERM=xterm-256color