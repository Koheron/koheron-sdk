project=oscillo

make clean
make NAME=$project

sh scripts/image.sh scripts/ubuntu.sh $project 1024
