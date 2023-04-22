#!/bin/sh
APPDIR="$(dirname -- "$(readlink -f -- "${0}")" )"
PRJDIR="$(readlink -f -- "$APPDIR/..")"
IMAGENAME="arduinosdk"

# path in docker image
XTENSAADDR2LIN="/root/.arduino15/packages/esp32/tools/xtensa-esp32-elf-gcc/esp-2021r2-patch5-8.4.0/bin/xtensa-esp32-elf-addr2line"
# name of the matching build output
ELFFILENAME="nuki_hub.elf"

# Check if at least one parameter was passed
if [ $# -eq 0 ]; then
    echo "Please provide a dump of exception (e.g. dump.txt) as first parameter"
    exit 1
fi

DUMPFILE="$1"
DUMPFILE_NAME="`date +%s`.txt"
DUMPFILE_COPY="$PRJDIR/build/$DUMPFILE_NAME"

# Check if the file exists
if [[ ! -e "$DUMPFILE" ]]; then    
    echo "The file $DUMPFILE does not exist"
    exit 1
fi

if [[ ! -d "$PRJDIR/build" ]] ; then
    echo "The dir $PRJDIR/build does not exist"
    exit 1
fi

if [[ ! -f "$PRJDIR/build/$ELFFILENAME" ]] ; then
    echo "The file $PRJDIR/build/$ELFFILENAME does not exist"
    exit 1
fi


if [[ ! -f "$APPDIR/EspStackTraceDecoder.jar" ]] ; then
    curl -L "https://github.com/littleyoda/EspStackTraceDecoder/releases/download/untagged-59a763238a6cedfe0362/EspStackTraceDecoder.jar" -o "$APPDIR/EspStackTraceDecoder.jar"
fi

if [[ "$(docker images -q $IMAGENAME 2> /dev/null)" == "" ]]; then
  echo "build docker image"
  docker build -t $IMAGENAME "$APPDIR"
fi

cp "$DUMPFILE" "$DUMPFILE_COPY"
docker run --rm -v $PRJDIR:/project:delegated -w /project/build $IMAGENAME java -jar ../Docker/EspStackTraceDecoder.jar "$XTENSAADDR2LIN" "$ELFFILENAME" "$DUMPFILE_NAME"
rm "$DUMPFILE_COPY"


