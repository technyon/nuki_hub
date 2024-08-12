set -ex
IMAGE_NAME=nuki_hub_build
docker build -f ./Dockerfile -t ${IMAGE_NAME} ..
docker run --rm -it -v $PWD/..:/src -w /src ${IMAGE_NAME} /bin/bash -c "make deps && make updater && make debug"