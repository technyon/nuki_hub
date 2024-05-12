set -ex
docker build -f ./Dockerfile_pio_debug -t nuki_hub_pio_dbg ..
docker create --name nuki_hub_pio_dbg nuki_hub_pio_dbg
rm -rf ../build_pio_dbg
docker cp nuki_hub_pio_dbg:/usr/src/nuki_hub/debug/ ../build_pio_dbg
docker rm -f nuki_hub_pio_dbg
