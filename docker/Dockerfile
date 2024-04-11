FROM gcc:9.5
RUN apt update
RUN apt install -y binutils-aarch64-linux-gnu binutils-arm-linux-gnueabi gcc-9-arm-linux-gnueabihf g++-9-arm-linux-gnueabihf gcc-9-aarch64-linux-gnu  g++-9-aarch64-linux-gnu
RUN apt install -y g++-arm-linux-gnueabihf
RUN apt install -y gcc-arm-none-eabi
RUN apt install -y flex bison bc u-boot-tools
ENV TERM=xterm-256color
USER containeruser

