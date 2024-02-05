set -ex
docker build -f ./Dockerfile -t nuki_hub ..
docker create --name nuki_hub nuki_hub
rm -rf ../build
docker cp nuki_hub:/usr/src/nuki_hub/build/ ../
docker rm -f nuki_hub
