set -ex
docker build -f ./Dockerfile_cmake -t nuki_hub_cmake ..
docker create --name nuki_hub_cmake nuki_hub_cmake
rm -rf ../build_cmake
docker cp nuki_hub_cmake:/usr/src/nuki_hub/build/ ../build_cmake
docker rm -f nuki_hub_cmake