FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ca-certificates wget unzip \
    binutils-aarch64-linux-gnu binutils-arm-linux-gnueabihf \
    gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf \
    gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
    gcc-arm-none-eabi \
    flex bison bc qemu-user-static zerofree u-boot-tools libgnutls28-dev \
    libeigen3-dev \
    libnewlib-arm-none-eabi \
    libssl-dev zlib1g-dev pkg-config \
    parted udev curl ca-certificates dosfstools e2fsprogs zip eatmydata\
    && rm -rf /var/lib/apt/lists/*

# Optional Eigen shim (see above)
RUN ln -s /usr/include/eigen3/Eigen /usr/include/Eigen && \
    ln -s /usr/include/eigen3/unsupported /usr/include/unsupported

ENV TERM=xterm-256color