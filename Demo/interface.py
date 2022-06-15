import serial, struct
import numpy as np
from time import sleep
import matplotlib.pyplot as plt
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

print('Midas touch found')
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
            print('Starting acquisition')
            StartSaving = True
            continue
        
        if 'EndMidas' in data:
            print('Acquisition complete')
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

delta_t = 4e-6
##  Plot
fig,ax = plt.subplots()
ax.plot(mapped_I)
ax.plot(mapped_V)


## plot average curve, 4micro s delta t, 3kHz signal

one_cycle_time = 1/3000
pts_in_one_cycle = int(one_cycle_time/delta_t)
cycles_in_data_set = int(len(mapped_I)/pts_in_one_cycle)
av_I = np.mean(np.split(mapped_I[:cycles_in_data_set*pts_in_one_cycle], cycles_in_data_set),axis=0)
av_V = np.mean(np.split(mapped_V[:cycles_in_data_set*pts_in_one_cycle], cycles_in_data_set),axis=0)
fig1, ax1 =plt.subplots()
ax1.plot([x*delta_t for x in range(pts_in_one_cycle)],av_I)
fig2, ax2 =plt.subplots()
ax2.plot([x*delta_t for x in range(pts_in_one_cycle)],av_V)

plt.show()