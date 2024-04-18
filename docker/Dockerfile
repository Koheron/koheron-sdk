FROM gcc:9.5
RUN apt update
RUN apt install -y binutils-aarch64-linux-gnu \
  binutils-arm-linux-gnueabi \
  gcc-9-arm-linux-gnueabihf \
  g++-9-arm-linux-gnueabihf \
  gcc-9-aarch64-linux-gnu \
  g++-9-aarch64-linux-gnu \
  g++-arm-linux-gnueabihf \
  gcc-arm-none-eabi \
  flex \
  bison \
  bc \
  u-boot-tools \
RUN wget -P /tmp https://gitlab.com/libeigen/eigen/-/archive/3.3.9/eigen-3.3.9.zip
RUN unzip -o /tmp/eigen-3.3.9.zip -d /tmp
RUN cp -r /tmp/eigen-3.3.9/unsupported /usr/include
RUN cp -r /tmp/eigen-3.3.9/Eigen /usr/include
ENV TERM=xterm-256color
USER containeruser

