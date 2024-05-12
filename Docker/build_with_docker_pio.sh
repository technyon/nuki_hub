set -ex
docker build -f ./Dockerfile_pio -t nuki_hub_pio ..
docker create --name nuki_hub_pio nuki_hub_pio
rm -rf ../build_pio
docker cp nuki_hub_pio:/usr/src/nuki_hub/release/ ../build_pio
docker rm -f nuki_hub_pio
