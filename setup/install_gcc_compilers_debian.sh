apt-get install -y g++-$1-arm-linux-gnueabihf
apt-get install -y g++-$1-aarch64-linux-gnu
update-alternatives --install /usr/bin/arm-linux-gnueabihf-g++ arm-linux-gnueabihf-g++ /usr/bin/arm-linux-gnueabihf-g++-${GCC_VERSION}  100
update-alternatives --install /usr/bin/arm-linux-gnueabihf-gcc arm-linux-gnueabihf-gcc /usr/bin/arm-linux-gnueabihf-gcc-${GCC_VERSION}  100
update-alternatives --install /usr/bin/arm-linux-gnueabihf-gcc-ar arm-linux-gnueabihf-gcc-ar /usr/bin/arm-linux-gnueabihf-gcc-ar-${GCC_VERSION}  100
update-alternatives --install /usr/bin/arm-linux-gnueabihf-gcov arm-linux-gnueabihf-gcov /usr/bin/arm-linux-gnueabihf-gcov-${GCC_VERSION}  100
update-alternatives --install /usr/bin/arm-linux-gnueabihf-cpp arm-linux-gnueabihf-cpp /usr/bin/arm-linux-gnueabihf-cpp-${GCC_VERSION}  100
update-alternatives --install /usr/bin/arm-linux-gnueabihf-gcc-nm arm-linux-gnueabihf-gcc-nm /usr/bin/arm-linux-gnueabihf-gcc-nm-${GCC_VERSION}  100
update-alternatives --install /usr/bin/arm-linux-gnueabihf-gcc-runlib arm-linux-gnueabihf-gcc-ranlib /usr/bin/arm-linux-gnueabihf-gcc-ranlib-${GCC_VERSION}  100
update-alternatives --install /usr/bin/arm-linux-gnueabihf-gcc-ranlib arm-linux-gnueabihf-gcc-ranlib /usr/bin/arm-linux-gnueabihf-gcc-ranlib-${GCC_VERSION}  100
update-alternatives --install /usr/bin/arm-linux-gnueabihf-gcov-dump arm-linux-gnueabihf-gcov-dump /usr/bin/arm-linux-gnueabihf-gcov-dump-${GCC_VERSION}  100
update-alternatives --install /usr/bin/arm-linux-gnueabihf-gcov-tool arm-linux-gnueabihf-gcov-tool /usr/bin/arm-linux-gnueabihf-gcov-tool-${GCC_VERSION}  100

update-alternatives --install /usr/bin/aarch64-linux-gnu-g++ aarch64-linux-gnu-g++ /usr/bin/aarch64-linux-gnu-g++-${GCC_VERSION}  100
update-alternatives --install /usr/bin/aarch64-linux-gnu-gcc aarch64-linux-gnu-gcc /usr/bin/aarch64-linux-gnu-gcc-${GCC_VERSION}  100
update-alternatives --install /usr/bin/aarch64-linux-gnu-gcc-ar aarch64-linux-gnu-gcc-ar /usr/bin/aarch64-linux-gnu-gcc-ar-${GCC_VERSION}  100
update-alternatives --install /usr/bin/aarch64-linux-gnu-gcov aarch64-linux-gnu-gcov /usr/bin/aarch64-linux-gnu-gcov-${GCC_VERSION}  100
update-alternatives --install /usr/bin/aarch64-linux-gnu-cpp aarch64-linux-gnu-cpp /usr/bin/aarch64-linux-gnu-cpp-${GCC_VERSION}  100
update-alternatives --install /usr/bin/aarch64-linux-gnu-gcc-nm arm-linux-gnueabihf-gcc-nm /usr/bin/arm-linux-gnueabihf-gcc-nm-${GCC_VERSION}  100
update-alternatives --install /usr/bin/aarch64-linux-gnu-gcc-runlib aarch64-linux-gnu-gcc-ranlib /usr/bin/aarch64-linux-gnu-gcc-ranlib-${GCC_VERSION}  100
update-alternatives --install /usr/bin/aarch64-linux-gnu-gcc-ranlib aarch64-linux-gnu-gcc-ranlib /usr/bin/aarch64-linux-gnu-gcc-ranlib-${GCC_VERSION}  100
update-alternatives --install /usr/bin/aarch64-linux-gnu-gcov-dump aarch64-linux-gnu-gcov-dump /usr/bin/aarch64-linux-gnu-gcov-dump-${GCC_VERSION}  100
update-alternatives --install /usr/bin/aarch64-linux-gnu-gcov-tool aarch64-linux-gnu-gcov-tool /usr/bin/aarch64-linux-gnu-gcov-tool-${GCC_VERSION}  100
