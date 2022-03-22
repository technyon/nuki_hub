# Crc16 A simple crc-16 library for Arduino

## Description
Use this library to implement crc checks on buffer arrays

## Usage
There are two modes to calculate crc: incremental and single call:
* In first mode the crc is calculated adding data bytes one by one and then calculating final crc, this is useful
for reception routines that receives bytes asynchrously,
* The second mode is used to obtain crc from a buffer array.
Using one mode doesn't interfere with the other (So you can calculate tx crc while receiving data and updating rx crc)

Is possible to configure crc with all crc-16bit standards (by default is defined XModem).

See possible crc variants:
http://www.lammertbies.nl/comm/info/crc-calculation.html 



