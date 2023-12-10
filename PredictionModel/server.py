import serial
import subprocess
import sys
import csv

#replace first param with serial port Arduino DB is currently connected to.
ser = serial.Serial('/dev/ttyACM0', 9600)

#server process to start main().
while True:
    try:
        #check if data in the serial buffer .
        if ser.in_waiting > 0:
            #read current data in serial buffer.
            command = ser.readline().decode().strip()
            if command == "run":
                #start the main process to produce predictions and read out.
                subprocess.run(['python3', 'client/main.py'])
            elif command.startswith("receive"):
                #starts receive process which writes new temp values to CSV files.
                parts = command.split("-")
                if len(parts) > 1:
                    try:
                        #get id, year, month, day, hour, and temperature into CSV file.
                        id = int(parts[1].strip())
                        year = parts[2].strip()
                        month = parts[3].strip()
                        day = parts[4].strip()
                        hour = parts[5].strip()
                        temperature = float(parts[6].strip())
                        #now add these values to the training data for station (id)
                        #id - 7 since addresses identifier increments up from 8.. (8, 9, 10...)
                        with open(f'training_sets/station{id - 7}.csv', 'a', newline='') as csvfile:
                            writer = csv.writer(csvfile)
                            writer.writerow([year, month, day, hour, temperature])
                    except ValueError:
                        print("Invalid temperature value.")

    except KeyboardInterrupt:
        sys.exit(130)
    