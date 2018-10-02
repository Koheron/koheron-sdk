#!/bin/bash

VERSION=c1dd3b016076

wget -P /tmp https://bitbucket.org/eigen/eigen/get/${VERSION}.zip
unzip -o /tmp/${VERSION}.zip -d /tmp
cp -r /tmp/eigen-eigen-${VERSION}/Eigen /usr/include