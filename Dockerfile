FROM ubuntu:16.04

ENV work_dir /code

# ---------------------------------------
# Install dependencies
# ---------------------------------------

RUN apt-get update
RUN apt-get -y install gcc-5 g++-5             \
                       gcc-arm-linux-gnueabihf \
                       g++-arm-linux-gnueabihf \
                       gcc-arm-linux-gnueabi   \
                       g++-arm-linux-gnueabi   \
                       make                    \
                       wget                    \
                       git                     \
                       python-pip              \
                       python-dev              \
                       build-essential         \
                       libyaml-dev             \
                       mingw-w64

RUN pip install --upgrade pip

WORKDIR $work_dir/
COPY . $work_dir/

RUN pip install -r requirements.txt

# ---------------------------------------
# Build project servers
# ---------------------------------------

RUN make NAME=oscillo tmp/oscillo.tcp-server/tmp/server/kserverd
RUN make NAME=spectrum tmp/spectrum.tcp-server/tmp/server/kserverd
RUN make NAME=pid tmp/pid.tcp-server/tmp/server/kserverd