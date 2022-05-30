import serial, struct
from time import sleep

class PICO_SERIAL:
    TERMINATOR = '\r'.encode('UTF8')

    def __init__(self, PORT, baudRate, timeout):
        self.serial = serial.Serial(PORT, baud, timeout=timeout)

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

    def __enter__(self):
        return self.serial

    def __exit__(self, type, value, traceback):
        self.serial.close()

PORT = '/dev/ttyACM0'
baudRate = 115200
timeout=.1

with seral.Serial(PORT, baudRate, timeout = timeout) as pico:

    sleep(1)

    while True:

        #data = pico.serial.readline() #the last bit gets rid of the new-line chars
        data = pico.receive()

        if data == 'Ready':
            break
    
    print('Starting acquisition')
    sleep(1)

    pico.start()


    while True:

        #data = pico.serial.readline()
        data = pico.receive()

        if data: #we got something
            print(f'data : {data}')