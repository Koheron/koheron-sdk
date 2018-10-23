#!/bin/bash

VERSION=b3f3d4950030

wget -P /tmp https://bitbucket.org/eigen/eigen/get/${VERSION}.zip
unzip -o /tmp/${VERSION}.zip -d /tmp
cp -r /tmp/eigen-eigen-${VERSION}/unsupported /usr/include
cp -r /tmp/eigen-eigen-${VERSION}/Eigen /usr/include
