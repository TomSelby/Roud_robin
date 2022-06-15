import serial, struct
import numpy as np
from time import sleep
####
class PICO_SERIAL:
    TERMINATOR = '\n'.encode('UTF8')

    def __init__(self, PORT, baudRate, timeout):
        self.PORT = PORT
        self.baudRate = baudRate
        self.timeout= timeout
        self.serial = serial.Serial(PORT, baudRate, timeout=timeout)

    def receive(self) -> str:
        line = self.serial.read_until(self.TERMINATOR)
        return line.decode('UTF8').strip()

    def send(self, text: str) -> bool:
        line = '%s\r' % text
        self.serial.write(line.encode('UTF8'))
        # the line should be echoed.
        # If it isn't, something is wrong.
        return 
    
    def start(self):
        self.serial.write('\r'.encode('UTF8'))

    def close(self):
        self.serial.close()
    
    def reconnect(self):
        self.close()
        self.serial.open()
        #print(self.serial.isOpen())
        self.serial = serial.Serial(self.PORT, self.baudRate, self.timeout)
        

    def __enter__(self):
        while True:
            try:
                return self
            except Exception:
                sleep(1)

    def __exit__(self, type, value, traceback):
        self.serial.close()

PORT = '/dev/ttyACM0'
baudRate = 12*921600
timeout=.1

#with PICO_SERIAL(PORT, baudRate, timeout = timeout) as pico:
pico = PICO_SERIAL(PORT, baudRate, timeout = timeout)
sleep(1)

#while True:

    #data = pico.serial.readline() #the last bit gets rid of the new-line chars
    #data = pico.receive()

    #if data == 'Ready':
        #break

print('Starting acquisition')
sleep(1)

#pico.start()

DataList = []
StartSaving = False
while True:

    #data = pico.serial.readline()
    try:
        data = pico.receive()

        #if data: #we got something
         #   print(f'data : {data}')
        
        if 'StartMidas' in data:
            StartSaving = True
            continue
        
        if 'EndMidas' in data:
            break
            
        if StartSaving and data:
            val = int(data, 16)
            DataList.append(val)

            
    except Exception as e:
        pico.close()
        
        print(type(e).__name__, e)
        print('lost')
        
        #sleep(1)
        try:
            pico.reconnect()
        except Exception:
            sleep(1)
    except KeyboardInterrupt:
        break

pico.close()
print('Data points', len(DataList))

# I didnt test/debug this but should work
array = np.array(DataList)# create a np array with all values in it
array_I = array[::2]# create an array with only I values in it- #Note the 'I values' are read as a voltage here
array_V  = array [1::2]# create an array with only the V values in it
mapped_I=(array_I/4095)*3.3# We want the current mapped to the original voltage applied
mapped_V= (array_V/4095)*3.3
np.savetxt('Current',mapped_I)
np.savetxt('Voltage', mapped_V)
