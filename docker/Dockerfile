FROM gcc:9.5
ARG GCC_VERSION=9
RUN apt update
RUN apt install -y binutils-aarch64-linux-gnu \
  binutils-arm-linux-gnueabi \
  gcc-${GCC_VERSION}-arm-linux-gnueabihf \
  g++-${GCC_VERSION}-arm-linux-gnueabihf \
  gcc-${GCC_VERSION}-aarch64-linux-gnu \
  g++-${GCC_VERSION}-aarch64-linux-gnu \
  gcc-arm-none-eabi \
  flex \
  bison \
  bc \
  qemu-user-static \
  zerofree \
  u-boot-tools \
  libgnutls28-dev

RUN update-alternatives --install /usr/bin/arm-linux-gnueabihf-g++ arm-linux-gnueabihf-g++ /usr/bin/arm-linux-gnueabihf-g++-${GCC_VERSION}  100
RUN update-alternatives --install /usr/bin/arm-linux-gnueabihf-gcc arm-linux-gnueabihf-gcc /usr/bin/arm-linux-gnueabihf-gcc-${GCC_VERSION}  100
RUN update-alternatives --install /usr/bin/arm-linux-gnueabihf-gcc-ar arm-linux-gnueabihf-gcc-ar /usr/bin/arm-linux-gnueabihf-gcc-ar-${GCC_VERSION}  100
RUN update-alternatives --install /usr/bin/arm-linux-gnueabihf-gcov arm-linux-gnueabihf-gcov /usr/bin/arm-linux-gnueabihf-gcov-${GCC_VERSION}  100
RUN update-alternatives --install /usr/bin/arm-linux-gnueabihf-cpp arm-linux-gnueabihf-cpp /usr/bin/arm-linux-gnueabihf-cpp-${GCC_VERSION}  100
RUN update-alternatives --install /usr/bin/arm-linux-gnueabihf-gcc-nm arm-linux-gnueabihf-gcc-nm /usr/bin/arm-linux-gnueabihf-gcc-nm-${GCC_VERSION}  100
RUN update-alternatives --install /usr/bin/arm-linux-gnueabihf-gcc-runlib arm-linux-gnueabihf-gcc-ranlib /usr/bin/arm-linux-gnueabihf-gcc-ranlib-${GCC_VERSION}  100
RUN update-alternatives --install /usr/bin/arm-linux-gnueabihf-gcc-ranlib arm-linux-gnueabihf-gcc-ranlib /usr/bin/arm-linux-gnueabihf-gcc-ranlib-${GCC_VERSION}  100
RUN update-alternatives --install /usr/bin/arm-linux-gnueabihf-gcov-dump arm-linux-gnueabihf-gcov-dump /usr/bin/arm-linux-gnueabihf-gcov-dump-${GCC_VERSION}  100
RUN update-alternatives --install /usr/bin/arm-linux-gnueabihf-gcov-tool arm-linux-gnueabihf-gcov-tool /usr/bin/arm-linux-gnueabihf-gcov-tool-${GCC_VERSION}  100

RUN update-alternatives --install /usr/bin/aarch64-linux-gnu-g++ aarch64-linux-gnu-g++ /usr/bin/aarch64-linux-gnu-g++-${GCC_VERSION}  100
RUN update-alternatives --install /usr/bin/aarch64-linux-gnu-gcc aarch64-linux-gnu-gcc /usr/bin/aarch64-linux-gnu-gcc-${GCC_VERSION}  100
RUN update-alternatives --install /usr/bin/aarch64-linux-gnu-gcc-ar aarch64-linux-gnu-gcc-ar /usr/bin/aarch64-linux-gnu-gcc-ar-${GCC_VERSION}  100
RUN update-alternatives --install /usr/bin/aarch64-linux-gnu-gcov aarch64-linux-gnu-gcov /usr/bin/aarch64-linux-gnu-gcov-${GCC_VERSION}  100
RUN update-alternatives --install /usr/bin/aarch64-linux-gnu-cpp aarch64-linux-gnu-cpp /usr/bin/aarch64-linux-gnu-cpp-${GCC_VERSION}  100
RUN update-alternatives --install /usr/bin/aarch64-linux-gnu-gcc-nm arm-linux-gnueabihf-gcc-nm /usr/bin/arm-linux-gnueabihf-gcc-nm-${GCC_VERSION}  100
RUN update-alternatives --install /usr/bin/aarch64-linux-gnu-gcc-runlib aarch64-linux-gnu-gcc-ranlib /usr/bin/aarch64-linux-gnu-gcc-ranlib-${GCC_VERSION}  100
RUN update-alternatives --install /usr/bin/aarch64-linux-gnu-gcc-ranlib aarch64-linux-gnu-gcc-ranlib /usr/bin/aarch64-linux-gnu-gcc-ranlib-${GCC_VERSION}  100
RUN update-alternatives --install /usr/bin/aarch64-linux-gnu-gcov-dump aarch64-linux-gnu-gcov-dump /usr/bin/aarch64-linux-gnu-gcov-dump-${GCC_VERSION}  100
RUN update-alternatives --install /usr/bin/aarch64-linux-gnu-gcov-tool aarch64-linux-gnu-gcov-tool /usr/bin/aarch64-linux-gnu-gcov-tool-${GCC_VERSION}  100
RUN wget -P /tmp https://gitlab.com/libeigen/eigen/-/archive/3.3.9/eigen-3.3.9.zip
RUN unzip -o /tmp/eigen-3.3.9.zip -d /tmp
RUN cp -r /tmp/eigen-3.3.9/unsupported /usr/include
RUN cp -r /tmp/eigen-3.3.9/Eigen /usr/include
RUN apt-get clean
ENV TERM=xterm-256color
USER containeruser
