import serial

ser = serial.Serial('/dev/ttyUSB0', 115200)
print(ser.portstr)

ser.write(b'nwhw 1 -1\r')
ser.write(b'wifi <ssid> <psk>\r')
ser.write(b'mqttbroker 192.168.0.10 1883\r')
ser.write(b'nukipath nukitstx nukioptstx\r')
ser.write(b'restart\r')

ser.close()