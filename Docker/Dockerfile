FROM eclipse-temurin:11-jdk-jammy AS builder

RUN set -ex &&         \
    apt-get update &&  \
    apt-get install -y git python3 pip make && \
    pip install --upgrade platformio && \
    pip install --upgrade esptool

CMD ["/bin/bash"]
