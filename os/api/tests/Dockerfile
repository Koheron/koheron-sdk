FROM ubuntu:26.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++-15 make pkg-config python3 libmicrohttpd-dev libzip-dev libjson-c-dev \
    libsystemd-dev libunistring-dev fdisk && rm -rf /var/lib/apt/lists/*
ENV NATIVE_API_BIN_DIR=tmp/native-api/amd64
CMD ["sh", "-ec", "make -j2 -f os/api/Makefile CXX=g++-15 BUILD_DIR=$NATIVE_API_BIN_DIR; python3 -m unittest discover -s os/api/tests -v; python3 -m unittest discover -s os/tests -p test_grow_rootfs.py -v; python3 -m unittest discover -s os/tests -p test_rootfs_settings_make.py -v; python3 -m unittest discover -s os/tests -p test_rootfs_overlay.py -v"]
