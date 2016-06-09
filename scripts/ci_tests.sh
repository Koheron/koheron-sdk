set -e

pip install -r requirements.txt

make NAME=oscillo tmp/oscillo.tcp-server/tmp//kserverd
make NAME=spectrum tmp/spectrum.tcp-server/tmp/kserverd
make NAME=pid tmp/pid.tcp-server/tmp/kserverd