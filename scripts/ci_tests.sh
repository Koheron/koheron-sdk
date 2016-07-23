set -e

pip install -r requirements.txt

make DOCKER=True NAME=oscillo server
make DOCKER=True NAME=spectrum server
make DOCKER=True NAME=pid server
