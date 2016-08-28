#!/bin/sh

branch=$1

mkdir -p /tmp/download
cd /tmp/download
curl -LOk https://github.com/Koheron/koheron-python/archive/${branch}.zip
unzip ${branch}.zip
cd koheron-python-${branch}
python setup.py install
rm -r /tmp/download
