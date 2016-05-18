set -e

pip install -r requirements.txt

make DOCKER=True NAME=oscillo tmp/oscillo.tcp-server/tmp/server/kserverd
make DOCKER=True NAME=spectrum tmp/spectrum.tcp-server/tmp/server/kserverd
make DOCKER=True NAME=pid tmp/pid.tcp-server/tmp/server/kserverd