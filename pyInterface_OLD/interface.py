import serial, struct

PORT = 'COM1'
baudRate = 115200
timeout=.1


pico = seral.Serial(PORT, baudRate, timeout = timeout)

while True:

	data = pico.readline() #the last bit gets rid of the new-line chars

    if data: #we got something
        print(f'data : {data}')