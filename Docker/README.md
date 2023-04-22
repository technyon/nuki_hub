# Development Docker 

Since setting up the toolchain can be difficult you can use this Docker-Environment to make own builds on mac and maybe linux.


Feel free and translate the shellscript to windows batch files  

## HowTo

Using it is relatively simple, change to the project directory and run the following script from the console:
```console
 ./Docker/build_with_docker.sh
```


## Requirements
- a running docker installation
- shell tools (sh, curl, ...)

Note: the used docker image "arduinosdk" is based on alpine linux and will created only by the first call. To recreate the image, you must manually remove it from your Docker repos.

## (goodie) ESP Stacktrace Decoding

I know, programming errors never happen, but if something does go wrong, you can analyze the CrashStack of the ESP with the following call:
```console
./Docker/decodestacktrace.sh name_of_your_dump.txt
```