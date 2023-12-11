import datetime
import serial 
from serial.tools import list_ports
import struct
from collections import namedtuple

#can use the below utility function to check which serial ports are available.
available_ports = list_ports.comports()

#testing - printing out available ports
PORT = str(available_ports[0])
for port in available_ports:
    print(str(port))

#initialising serial port and prediction struct to pack over to snapshot DB.
serialPort = serial.Serial (port = PORT, baudrate = 9600, bytesize = 8, timeout = 2, stopbits = serial.STOPBITS_ONE)
Prediction = namedtuple ('Prediction', ['id', 'day', 'prediction'])


#pack into binary string and send over Serial.
def send_predictions(future_predictions):
    current_date = datetime.datetime.now()
    for i, StationPrediction in enumerate(future_predictions):
        future_temperatures = StationPrediction.predictions
        for temp in future_temperatures:
            pred = Prediction(id = StationPrediction.id, day = (current_date + datetime.timedelta(days = i)).strftime("%A"), prediction = temp)
            #send string of size 10 and two unsigned integers.
            b_string = struct.pack('I10sI', pred.day.encode(), pred.prediction)
            serialPort.write(b_string)
            serialPort.close()