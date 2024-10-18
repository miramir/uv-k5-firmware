#!/bin/sh
IMAGE_NAME="uvk5-build"
docker build -t $IMAGE_NAME .
docker run --rm --volume /etc/passwd:/etc/passwd:ro --volume /etc/group:/etc/group:ro --user $(id -u) \
  -v "${PWD}:/app" $IMAGE_NAME /bin/sh -c "cd /app && git submodule update --init --recursive && make clean && make"