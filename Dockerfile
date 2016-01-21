FROM ubuntu:14.04

ENV work_dir /code
                       
RUN apt-get -y install g++-arm-linux-gnueabihf \
                       make \
                       git

# Install virtualenv
RUN apt-get -y install python-pip
RUN pip install --upgrade pip

WORKDIR $work_dir/
COPY . $work_dir/

RUN pip install -r requirements.txt

RUN make tcp-server
