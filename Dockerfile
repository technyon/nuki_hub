FROM alpine:latest
RUN apk add --update --no-cache bash cmake curl python3 openjdk8 nano git make gcompat libc6-compat py3-pyserial
RUN curl -L "https://downloads.arduino.cc/arduino-1.8.19-linux`uname -m`.tar.xz" -o /tmp/arduino-ide.tar.xz
RUN tar -xvf /tmp/arduino-ide.tar.xz --directory ~/

RUN chown -R root ~/arduino-*
RUN chgrp -R root ~/arduino-*

RUN cd ~/arduino* && rm -r java

RUN cd ~/arduino* && ln -s /usr/lib/jvm/java-8-openjdk java
RUN cd ~/arduino* && ./install.sh
RUN cd ~/arduino* && ./arduino --pref "boardsmanager.additional.urls=https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json" --save-prefs
RUN cd ~/arduino* && ./arduino --install-boards esp32:esp32

CMD [ "/bin/bash" ]