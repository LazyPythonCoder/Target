import subprocess
import time
import signal
import sys,os

# command = "mavproxy.exe --master=COM3 --baudrate=57600 --out=udp:127.0.0.1:14550 --out=udp:127.0.0.1:14551"

# subprocess.run(['C:\Program Files (x86)\MAVProxy\mavproxy.exe --master=COM3 --baudrate=57600 --out=udp:127.0.0.1:14550 --out=udp:127.0.0.1:14551'], capture_output=True, text=True)

# Path to the .bat file

# Path to the .bat file
def start_bat_file():
    bat_file_path = "mav.bat"

# Run the .bat file
    try:
        completed_process = subprocess.run([bat_file_path], check=True, shell=True)
        print("The .bat file ran successfully")
        print("Return code:", completed_process.returncode)
    except subprocess.CalledProcessError as e:
        print("An error occurred while running the .bat file")
        print("Return code:", e.returncode)


if __name__ == "__main__":
    start_bat_file()
