#!/bin/bash

VERSION=3.3.9

wget -P /tmp https://gitlab.com/libeigen/eigen/-/archive/${VERSION}/eigen-${VERSION}.zip
unzip -o /tmp/eigen-${VERSION}.zip -d /tmp
cp -r /tmp/eigen-${VERSION}/unsupported /usr/include
cp -r /tmp/eigen-${VERSION}/Eigen /usr/include

