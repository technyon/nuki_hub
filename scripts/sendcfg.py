import serial
import time
import sys
import os

def send_file_to_serial(file_path, port, baudrate=9600, delay=0.1):
    try:
        # Open serial port
        with serial.Serial(port, baudrate, timeout=1) as ser:
            print(f"Opened serial port {port} at {baudrate} baud.")

            # Read file and send line by line
            ser.write("-- NUKI HUB CONFIG START --\n".encode('utf-8'))

            with open(file_path, 'r') as file:
                for line in file:
                    ser.write(line.encode('utf-8'))  # Send line
                    ser.write("\n".encode('utf-8'))
                    print(f"Sent: {line.strip()}")
                    time.sleep(delay)  # Optional delay between lines

            ser.write("-- NUKI HUB CONFIG END --\n".encode('utf-8'))

            print("Configuration sent successfully.")

    except serial.SerialException as e:
        print(f"Serial error: {e}")
    except FileNotFoundError:
        print(f"File not found: {file_path}")
    except Exception as e:
        print(f"Unexpected error: {e}")

if __name__ == "__main__":

    if(len(sys.argv) < 2):
        print("Usage: sendcfg.py <port> <file>")
        sys.exit(1)

    config_file = sys.argv[2]
    if(not os.path.isfile(config_file)):
        print(f"File not found: {config_file}")
        sys.exit(2)

    serial_port = sys.argv[1]
    baud_rate = 115200

    send_file_to_serial(config_file, serial_port, baud_rate)
